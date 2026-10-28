/// The syntax check: every entry of catter's database compiled again with
/// `-fsyntax-only`, in its directory and with the environment its command
/// ran with. A database whose commands do not compile is wrong whatever
/// else agrees with it.

import * as os from "node:os";
import * as path from "node:path";
import * as fs from "node:fs";
import { runProcess } from "../../testing/process.ts";
import { baseName, isCompiler, withoutDependencyFiles } from "../command.ts";
import { quote, type Difference, type Places } from "../difference.ts";
import type { Command, Session } from "../session.ts";
import type { Entry } from "./recorder.ts";

/// The entry's arguments checking its source's code only, writing neither
/// its object nor its dependency files, or undefined for
/// a compiler that does not take GCC's options (cl, nvcc).
export function syntaxOnly(args: readonly string[]): string[] | undefined {
    const [program, ...rest] = args;
    if (program === undefined || !isCompiler(program) || /^(?:cl|clang-cl|nvcc)(?:\.exe)?$/i.test(baseName(program))) {
        return undefined;
    }
    return [program, ...withoutDependencyFiles(rest, ["-o"]).filter((arg) => arg !== "-c"), "-fsyntax-only"];
}

function environment(env: readonly string[]): NodeJS.ProcessEnv {
    return Object.fromEntries(
        env.map((entry) => {
            const at = entry.indexOf("=");
            return [entry.slice(0, at), entry.slice(at + 1)];
        }),
    );
}

export interface SyntaxStats {
    checked: number;
    passed: number;
    /// Entries whose source the build removed.
    gone: number;
    /// Entries of a compiler whose options are not GCC's.
    skipped: number;
}

/// The command each entry came from: the compile in its directory writing
/// its output, or naming its source.
function commandsByOutput(session: Session): Map<string, Command> {
    const map = new Map<string, Command>();
    for (const command of session.commands) {
        if (command.analysis?.kind !== "compiler") {
            continue;
        }
        for (const write of command.analysis.writes) {
            if (typeof write.path === "string") {
                map.set(path.resolve(command.cwd, write.path), command);
            }
        }
    }
    return map;
}

export async function checkSyntax(input: {
    entries: readonly Entry[];
    session: Session;
    places: Places;
}): Promise<{ differences: Difference[]; stats: SyntaxStats }> {
    const differences: Difference[] = [];
    const stats: SyntaxStats = { checked: 0, passed: 0, gone: 0, skipped: 0 };
    const byOutput = commandsByOutput(input.session);
    const queue = [...input.entries];
    const worker = async () => {
        for (let entry = queue.shift(); entry !== undefined; entry = queue.shift()) {
            const args = syntaxOnly(entry.arguments);
            if (args === undefined) {
                stats.skipped += 1;
                continue;
            }
            if (!fs.existsSync(path.resolve(entry.directory, entry.file))) {
                stats.gone += 1;
                continue;
            }
            const output = entry.output === undefined ? undefined : path.resolve(entry.directory, entry.output);
            const command =
                (output === undefined ? undefined : byOutput.get(output)) ??
                input.session.commands.find(
                    (each) => each.cwd === entry.directory && each.args.some((arg) => path.resolve(each.cwd, arg) === path.resolve(entry.directory, entry.file)),
                );
            stats.checked += 1;
            const [program = "", ...rest] = args;
            // Hang detection: no compile of one source takes ten minutes.
            const result = await runProcess(program, rest, {
                cwd: entry.directory,
                env: command === undefined ? process.env : environment(command.env),
                timeout: 600_000,
            });
            if (result.status === 0) {
                stats.passed += 1;
                continue;
            }
            const why = result.error?.message ?? result.stderr.split("\n").find((line) => /error/i.test(line)) ?? `exit ${result.status ?? result.signal}`;
            differences.push({
                check: "syntax",
                kind: "fails",
                detail: input.places.spell(`${path.resolve(entry.directory, entry.file)}: ${why.trim()}`),
                repro: input.places.spell(`cd ${quote(entry.directory)} && ${args.map(quote).join(" ")}`),
            });
        }
    };
    await Promise.all(Array.from({ length: os.availableParallelism() }, worker));
    return { differences, stats };
}
