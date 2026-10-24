/// What a scenario is handed, `t`, and how one registered test runs and
/// leaves its output.

import * as fs from "node:fs";
import * as path from "node:path";
import { withTimeout } from "../../promise.ts";
import {
    catterExecutable,
    commandLabel,
    normalize,
    runCommand,
    splitCommand,
    transcript,
    type CommandOptions,
    type Places,
} from "../cli/run.ts";
import { failureExcerpt, readLogs, undeclaredErrors } from "../logs/logs.ts";
import { mapText, type Table, type Value } from "../render/value.ts";
import { SCRIPT_DEADLINE_MS, StepRecord, throwFailures } from "../steps.ts";
import type { Case } from "./case.ts";
import { Disk } from "./disk.ts";
import { Output, type Blocks } from "./output.ts";
import { freshDir, linkSdk, materialize } from "./project.ts";
import type { Registered } from "./script.ts";

/// What a command left: its exit status, or the signal that killed it, and
/// what it printed, as it printed it.
export interface CommandRun {
    status: number | null;
    signal: NodeJS.Signals | null;
    stdout: string;
    stderr: string;
}

export interface TestContext {
    /// The copy's root.
    readonly root: string;
    /// Run catter in the copy; its transcript joins the output.
    cli(command: string | readonly string[], options?: CommandOptions): Promise<CommandRun>;
    /// Run another program in the copy, as a build does; its transcript
    /// joins the output.
    run(command: string | readonly string[], options?: CommandOptions): Promise<CommandRun>;
    /// Prepare the copy with a program whose output the test does not pin
    /// (a build's configure step): it must succeed, and what it printed shows
    /// only when it fails.
    setup(command: string | readonly string[], options?: CommandOptions): Promise<CommandRun>;
    /// A block of the output: text, lines or a number under `value`; a table
    /// as it is, `t.show("compile_commands.json", t.disk.cdb())`.
    show(label: string, value: Value): void;
    readonly disk: Disk;
}

function words(command: string | readonly string[]): string[] {
    return typeof command === "string" ? splitCommand(command) : [...command];
}

class TestRun implements TestContext {
    readonly root: string;
    readonly output = new Output();
    readonly disk: Disk;
    private readonly places: Places;
    private readonly steps = new StepRecord();
    private readonly errors: RegExp[];
    /// Whether catter ran with the test's data directory, where it then logs.
    private ranCatter = false;
    private readonly dirs: string[];

    constructor(c: Case, registered: Registered) {
        this.root = freshDir("catter-case-");
        // catter's data directory sits beside the copy, not in it: a build
        // never sees catter's logs.
        const home = freshDir("catter-home-");
        this.dirs = [this.root, home];
        this.places = { root: this.root, home };
        this.disk = new Disk(this.root, this.steps, (text) => normalize(text, this.places));
        this.errors = (registered.options.errors ?? []).map((source) => new RegExp(source));
        materialize(c.dir, this.root);
        linkSdk(this.root);
    }

    private async command(
        program: string,
        label: string,
        args: readonly string[],
        options: CommandOptions,
    ): Promise<CommandRun> {
        const result = await this.steps.run(label, () =>
            runCommand(program, args, this.places, options),
        );
        return {
            status: result.status,
            signal: result.signal,
            stdout: result.stdout,
            stderr: result.stderr,
        };
    }

    async cli(
        command: string | readonly string[],
        options: CommandOptions = {},
    ): Promise<CommandRun> {
        const args = words(command);
        const label = commandLabel("catter", args, this.places);
        const key = this.output.reserve(label);
        this.ranCatter ||= options.env?.["CATTER_HOME"] === undefined;
        const run = await this.command(catterExecutable(), label, args, options);
        this.output.transcript(key, transcript(run, this.places));
        return run;
    }

    async run(
        command: string | readonly string[],
        options: CommandOptions = {},
    ): Promise<CommandRun> {
        const [program, ...args] = words(command);
        if (program === undefined) {
            throw new Error("t.run() needs a program");
        }
        const label = commandLabel(program, args, this.places);
        const key = this.output.reserve(label);
        const run = await this.command(program, label, args, options);
        this.output.transcript(key, transcript(run, this.places));
        return run;
    }

    async setup(
        command: string | readonly string[],
        options: CommandOptions = {},
    ): Promise<CommandRun> {
        const [program, ...args] = words(command);
        if (program === undefined) {
            throw new Error("t.setup() needs a program");
        }
        const label = `setup ${commandLabel(program, args, this.places)}`;
        const run = await this.command(program, label, args, options);
        if (run.status !== 0) {
            throw new Error(
                `${label} ended with ${run.status === null ? String(run.signal) : `exit ${String(run.status)}`}\nstdout:\n${run.stdout}\nstderr:\n${run.stderr}`,
            );
        }
        return run;
    }

    show(label: string, value: Value): void {
        this.steps.note(`show ${label}`);
        const table: Table =
            typeof value === "object" && !Array.isArray(value) ? (value as Table) : { value };
        this.output.block(label, mapText(table, (text) => normalize(text, this.places)) as Table);
    }

    /// The gates every test ends with; returns what failed.
    finish(failed: boolean): unknown[] {
        const logs = readLogs(this.places.home);
        if (failed) {
            console.log(`steps:\n${this.steps.render()}`);
            const excerpt = failureExcerpt(logs);
            if (excerpt !== "") {
                console.log(`catter warnings and errors:\n${excerpt}`);
            }
        }
        const errors: unknown[] = [];
        // An empty log passes every check below: catter must have written one.
        if (this.ranCatter && !fs.existsSync(path.join(this.places.home, "log", "catter.log"))) {
            errors.push(new Error("catter wrote no log under ${CATTER_HOME}/log"));
        }
        const undeclared = undeclaredErrors(logs, this.errors);
        if (undeclared.length > 0) {
            errors.push(
                new Error(
                    `catter logged errors the test does not declare (errors: [...]):\n${undeclared
                        .map((record) => `${record.file}: ${record.message}`)
                        .join("\n")}`,
                ),
            );
        }
        if (process.env["CATTER_KEEP_WORKSPACE"] === undefined) {
            for (const dir of this.dirs) {
                fs.rmSync(dir, { recursive: true, force: true });
            }
        } else {
            console.log(`workspace kept at ${this.root}, catter's data at ${this.places.home}`);
        }
        return errors;
    }
}

/// Run one registered test of a case; returns its blocks.
export async function runRegistered(c: Case, registered: Registered): Promise<Blocks> {
    const run = new TestRun(c, registered);
    let failure: { error: unknown } | undefined;
    try {
        await withTimeout(
            registered.fn(run),
            SCRIPT_DEADLINE_MS,
            `${path.basename(c.dir)}: the test hung`,
        );
    } catch (error) {
        failure = { error };
    }
    throwFailures(failure, run.finish(failure !== undefined));
    return run.output.blocks;
}
