/// A job: a project's build on this platform, under each of its backends in
/// turn. Each backend builds a fresh copy of the source inside a session of
/// catter, a recorder around it, and the checks compare what catter
/// answered with the references (ecosystem/README.md, "A job").

import { spawn } from "node:child_process";
import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { undeclaredErrors, readLogs } from "../testing/logs/logs.ts";
import { REPO_ROOT } from "../repo.ts";
import { checkArtifacts, snapshot, type ArtifactStats } from "./checks/artifacts.ts";
import { checkHint, readDatabase } from "./checks/hint.ts";
import { checkRecorder, type Entry, type RecorderStats } from "./checks/recorder.ts";
import { checkSyntax, type SyntaxStats } from "./checks/syntax.ts";
import { isKnown, Places, quote, type Difference } from "./difference.ts";
import {
    currentPlatform,
    findBuild,
    type Backend,
    type Build,
    type Known,
    type Platform,
    type Step,
} from "./project.ts";
import { AuditRecorder } from "./record/audit.ts";
import { NoRecorder, type Recorder, type RecorderName } from "./record/recorder.ts";
import { StraceRecorder } from "./record/strace.ts";
import { readSession, replayOf } from "./session.ts";
import { copyTree, sourceOf } from "./source.ts";

export interface JobOptions {
    /// Where projects are fetched and built.
    dir: string;
    /// Where results, logs and what catter wrote go.
    out: string;
    catter: string;
    recorder: RecorderName;
    /// The backends to run, of the build's; all of them when empty.
    backends: Backend[];
    jobs: number;
}

export interface Found extends Difference {
    known?: { issue?: string; why: string };
}

export interface BackendResult {
    backend: Backend;
    recorder: RecorderName;
    /// Why the backend's run proves nothing about catter: a step outside
    /// the session failed, or the build fails without catter too.
    setup?: string;
    exit?: number | null;
    entries: number;
    recorded?: RecorderStats;
    artifacts?: ArtifactStats;
    syntax?: SyntaxStats;
    differences: Found[];
    repro: string;
}

export interface JobResult {
    job: string;
    platform: Platform;
    catter: string;
    backends: BackendResult[];
    /// Known differences of the job no backend showed: fixed, or hidden.
    unseen: { issue?: string; why: string; check: string; kind: string; detail: string }[];
    /// Why the job stopped before it checked anything.
    error?: string;
}

/// The places a step's words and the build's environment name.
function expand(word: string, places: { tree: string; work: string; jobs: number }): string {
    return word
        .replaceAll("${TREE}", places.tree)
        .replaceAll("${WORK}", places.work)
        .replaceAll("${JOBS}", String(places.jobs));
}

/// Run `command` to its end, its output appended to `log`; killed with
/// everything it started once `timeout` passed.
function runLogged(
    command: readonly string[],
    options: { cwd: string; env: NodeJS.ProcessEnv; log: string; timeout: number },
): Promise<number | null> {
    const [program = "", ...args] = command;
    fs.appendFileSync(options.log, `$ ${command.map(quote).join(" ")}\n`);
    const fd = fs.openSync(options.log, "a");
    return new Promise((resolve) => {
        const child = spawn(program, args, {
            cwd: options.cwd,
            env: options.env,
            stdio: ["ignore", fd, fd],
            detached: true,
        });
        // Hang detection: a bound only a build stuck forever reaches.
        const timer = setTimeout(() => {
            fs.appendFileSync(options.log, `\nkilled after ${options.timeout / 60_000} minutes\n`);
            try {
                process.kill(-(child.pid ?? 0), "SIGKILL");
            } catch {
                // Gone already.
            }
        }, options.timeout);
        child.on("error", (error) => {
            fs.appendFileSync(options.log, `${error.message}\n`);
        });
        child.on("close", (status) => {
            clearTimeout(timer);
            fs.closeSync(fd);
            resolve(status);
        });
    });
}

const STEP_TIMEOUT = 120 * 60_000;

class SetupError extends Error {}

async function runSteps(
    steps: readonly Step[],
    what: string,
    options: { cwd: string; env: NodeJS.ProcessEnv; log: string },
    places: { tree: string; work: string; jobs: number },
): Promise<void> {
    for (const step of steps) {
        const command = step.map((word) => expand(word, places));
        const status = await runLogged(command, { ...options, timeout: STEP_TIMEOUT });
        if (status !== 0) {
            throw new SetupError(
                `${what} step ${command.map(quote).join(" ")} failed (${status ?? "killed"}); its output is in ${options.log}`,
            );
        }
    }
}

function tail(file: string, lines = 30): string {
    return fs.existsSync(file)
        ? fs.readFileSync(file, "utf8").trimEnd().split("\n").slice(-lines).join("\n")
        : "";
}

function makeRecorder(name: RecorderName, work: string, tree: string): Recorder {
    switch (name) {
        case "auditd":
            return new AuditRecorder();
        case "strace":
            return new StraceRecorder(work, tree);
        case "none":
            return new NoRecorder();
    }
}

async function runBackend(
    job: string,
    build: Build,
    backend: Backend,
    source: string,
    options: JobOptions,
): Promise<BackendResult> {
    const root = path.join(options.dir, ...job.split("/"), backend);
    const out = path.join(options.out, backend);
    fs.rmSync(root, { recursive: true, force: true });
    fs.rmSync(out, { recursive: true, force: true });
    fs.mkdirSync(path.join(root, "work"), { recursive: true });
    fs.mkdirSync(out, { recursive: true });
    const tree = path.join(fs.realpathSync.native(root), "tree");
    const work = path.join(fs.realpathSync.native(root), "work");
    const places = { tree, work, jobs: options.jobs };
    const home = path.join(fs.realpathSync.native(out), "home");
    const env: NodeJS.ProcessEnv = {
        ...process.env,
        ...Object.fromEntries(
            Object.entries(build.env).map(([name, value]) => [name, expand(value, places)]),
        ),
    };
    const log = path.join(out, "build.log");
    const repro = `CATTER_EXECUTABLE=${quote(options.catter)} node tools/ecosystem/run.ts ${job} --backend ${backend} --dir ${quote(options.dir)}`;
    const result: BackendResult = {
        backend,
        recorder: options.recorder,
        entries: 0,
        differences: [],
        repro,
    };
    const spell = new Places({ tree, work, home, catter: path.dirname(options.catter) });
    const prepared = async (into: string) => {
        copyTree(source, into);
        await runSteps(
            build.prepare,
            "prepare",
            { cwd: into, env, log },
            { ...places, tree: into },
        );
    };

    try {
        await prepared(tree);
        if (backend !== "wrapper") {
            await runSteps(build.configure, "configure", { cwd: tree, env, log }, places);
        }
    } catch (error) {
        if (!(error instanceof SetupError)) {
            throw error;
        }
        result.setup = error.message;
        return result;
    }

    const steps = [...(backend === "wrapper" ? build.configure : []), ...build.build].map((step) =>
        step.map((word) => expand(word, places)),
    );
    const [only] = steps;
    const command =
        steps.length === 1 && only !== undefined
            ? only
            : [
                  process.execPath,
                  path.join(REPO_ROOT, "tools", "ecosystem", "steps.ts"),
                  JSON.stringify(steps),
              ];
    const cdb = path.join(out, "compile_commands.json");
    const events = path.join(out, "events.jsonl");
    const handler = path.join(REPO_ROOT, "tools", "ecosystem", "handler.ts");
    const before = snapshot(tree, build.scan);
    const recorder = makeRecorder(options.recorder, work, tree);
    recorder.start();
    result.exit = await runLogged(
        recorder.wrap([
            options.catter,
            "compile-commands",
            "--capture",
            backend,
            "-o",
            cdb,
            "--handler",
            [process.execPath, handler, events].map(quote).join(" "),
            "--",
            ...command,
        ]),
        { cwd: tree, env: { ...env, CATTER_HOME: home }, log, timeout: STEP_TIMEOUT },
    );
    const recording = await recorder.stop();
    fs.writeFileSync(path.join(out, "recording.json"), JSON.stringify(recording));
    const after = snapshot(tree, build.scan);

    const session = readSession(events);
    fs.writeFileSync(path.join(out, "replay.json"), JSON.stringify(replayOf(session), null, 1));
    const entries: Entry[] = fs.existsSync(cdb) ? readDatabase(cdb) : [];
    result.entries = entries.length;
    const add = (differences: readonly Difference[]) => {
        result.differences.push(...differences);
    };
    const catter = (kind: string, detail: string) => {
        add([{ check: "catter", kind, detail: spell.spell(detail) }]);
    };

    if (result.exit !== 0) {
        // The build again, without catter: a build that fails there too is
        // the project's, or the machine's.
        const plain = path.join(root, "plain");
        try {
            await prepared(plain);
            await runSteps(
                [...build.configure, ...build.build],
                "plain build",
                { cwd: plain, env, log: path.join(out, "plain.log") },
                { ...places, tree: plain },
            );
        } catch (error) {
            if (!(error instanceof SetupError)) {
                throw error;
            }
            result.setup = `the build fails without catter too: ${error.message}`;
            return result;
        } finally {
            fs.rmSync(plain, { recursive: true, force: true });
        }
        catter(
            "exit",
            `catter exited ${result.exit ?? "killed"}, the build without it 0:\n${tail(log)}`,
        );
    }
    if (session.finish === undefined) {
        catter("no-finish", "the handler was never told that the build ended");
    }
    for (const error of session.errors) {
        catter("capture-error", error.message);
    }
    for (const record of undeclaredErrors(readLogs(home), [])) {
        catter("log-error", record.message.split("\n")[0] ?? "");
    }
    const targetsHome = path.join(out, "targets-home");
    const targets = await runLogged(
        [options.catter, "targets", "--from", path.join(out, "replay.json")],
        {
            cwd: tree,
            env: { ...process.env, CATTER_HOME: targetsHome },
            log: path.join(out, "targets.txt"),
            timeout: STEP_TIMEOUT,
        },
    );
    if (targets !== 0) {
        catter("targets", `catter targets --from replay.json exited ${targets ?? "killed"}`);
    }
    for (const record of undeclaredErrors(readLogs(targetsHome), [])) {
        catter("targets", record.message.split("\n")[0] ?? "");
    }

    if (options.recorder !== "none") {
        const checked = checkRecorder({
            recording,
            session,
            entries,
            backend,
            catter: fs.realpathSync.native(options.catter),
            own: [home],
            handler,
            places: spell,
        });
        add(checked.differences);
        result.recorded = checked.stats;
    }
    const artifacts = checkArtifacts({ before, after, entries, session, places: spell });
    add(artifacts.differences);
    result.artifacts = artifacts.stats;
    const syntax = await checkSyntax({ entries, session, places: spell });
    add(syntax.differences);
    result.syntax = syntax.stats;
    add(
        checkHint({
            entries,
            hint: build.hint === undefined ? undefined : path.join(tree, build.hint),
            home,
            places: spell,
        }),
    );
    return result;
}

export async function runJob(job: string, options: JobOptions): Promise<JobResult> {
    const { project, build } = findBuild(job);
    const platform = currentPlatform();
    if (!build.platforms.includes(platform)) {
        throw new Error(`${job} does not build on ${platform}`);
    }
    const backends = options.backends.length === 0 ? build.backends : options.backends;
    for (const backend of backends) {
        if (!build.backends.includes(backend)) {
            throw new Error(`${job} does not run under ${backend}: ${build.backends.join(", ")}`);
        }
    }
    const source = await sourceOf(project, build, options.dir);
    const result: JobResult = { job, platform, catter: options.catter, backends: [], unseen: [] };
    const seen = new Set<Known>();
    for (const backend of backends) {
        console.log(`== ${job} under ${backend}`);
        const ran = await runBackend(job, build, backend, source, {
            ...options,
            out: path.join(options.out),
        });
        const where = { build: build.name, backend };
        for (const difference of ran.differences) {
            const known = project.known.find((each) => isKnown(each, difference, where));
            if (known !== undefined) {
                seen.add(known);
                difference.known = {
                    ...(known.issue === undefined ? {} : { issue: known.issue }),
                    why: known.why,
                };
            }
        }
        result.backends.push(ran);
    }
    result.unseen = project.known
        .filter(
            (known) =>
                !seen.has(known) &&
                (known.build === undefined || known.build === build.name) &&
                (known.backend === undefined || backends.includes(known.backend)),
        )
        .map((known) => ({
            ...(known.issue === undefined ? {} : { issue: known.issue }),
            why: known.why,
            check: known.check,
            kind: known.kind,
            detail: known.detail.source,
        }));
    return result;
}

export function defaultJobs(): number {
    return os.availableParallelism();
}
