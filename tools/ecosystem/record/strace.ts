/// strace as the recorder, where auditd is not there (a container, a machine
/// without sudo): it traces catter and everything it starts, a file per
/// thread. It slows a build down 1.75 times, and breaks a sanitized catter
/// (LeakSanitizer does not work under ptrace), setuid programs and programs
/// that trace others.

import * as fs from "node:fs";
import * as path from "node:path";
import { ImageBuilder, type Recording } from "./images.ts";
import type { Recorder } from "./recorder.ts";

const LINE = /^(\d+\.\d+) (\w+)\((.*)\) += (-?\d+|\?)/;

/// The C string literal at `text[start]`, a `"`: its value and the index
/// after it. strace escapes what is not printable as `\xNN`, `\n`, `\"`.
function cString(text: string, start: number): [string, number] {
    const bytes: number[] = [];
    let i = start + 1;
    const simple: Record<string, number> = { n: 10, t: 9, r: 13, v: 11, f: 12, '"': 34, "\\": 92 };
    while (text[i] !== '"' && i < text.length) {
        const char = text[i] ?? "";
        if (char !== "\\") {
            bytes.push(...Buffer.from(char, "latin1"));
            i += 1;
            continue;
        }
        const next = text[i + 1] ?? "";
        if (next === "x") {
            bytes.push(parseInt(text.slice(i + 2, i + 4), 16));
            i += 4;
        } else if (/[0-7]/.test(next)) {
            const octal = /^[0-7]{1,3}/.exec(text.slice(i + 1))?.[0] ?? "0";
            bytes.push(parseInt(octal, 8));
            i += 1 + octal.length;
        } else {
            bytes.push(simple[next] ?? next.charCodeAt(0));
            i += 2;
        }
    }
    return [Buffer.from(bytes).toString("utf8"), i + 1];
}

/// The top-level strings and string arrays of a system call's arguments.
export function straceStrings(text: string): (string | string[])[] {
    const items: (string | string[])[] = [];
    let array: string[] | undefined;
    for (let i = 0; i < text.length;) {
        const char = text[i];
        if (char === '"') {
            const [value, next] = cString(text, i);
            (array ?? items).push(value);
            i = next;
            continue;
        }
        if (char === "[" && array === undefined) {
            array = [];
        } else if (char === "]" && array !== undefined) {
            items.push(array);
            array = undefined;
        }
        i += 1;
    }
    return items;
}

interface Call {
    time: number;
    tid: number;
    name: string;
    args: string;
    result: string;
}

/// The images of `strace -ff -ttt` output files `<dir>/t.<tid>`: each
/// process's directory rebuilt from the first one's, `cwd`, its changes and
/// what a child inherits.
export function parseStrace(dir: string, cwd: string): ImageBuilder {
    const calls: Call[] = [];
    for (const name of fs.readdirSync(dir)) {
        const tid = Number(name.slice(name.lastIndexOf(".") + 1));
        // strace writes the bytes of a path as they are, escaping only what is
        // not printable: latin1 keeps every byte as one character.
        for (const line of fs.readFileSync(path.join(dir, name), "latin1").split("\n")) {
            const match = LINE.exec(line);
            if (match !== null) {
                const [, time = "0", call = "", args = "", result = ""] = match;
                calls.push({ time: Number(time), tid, name: call, args, result });
            }
        }
    }
    calls.sort((a, b) => a.time - b.time);
    const builder = new ImageBuilder();
    const threadOf = new Map<number, number>();
    const parentOf = new Map<number, number>();
    const cwds = new Map<number, string>();
    const processOf = (tid: number): number => threadOf.get(tid) ?? tid;
    const [first] = calls;
    if (first !== undefined) {
        cwds.set(first.tid, cwd);
    }
    for (const call of calls) {
        const pid = processOf(call.tid);
        const ok = call.result !== "?" && Number(call.result) >= 0;
        if (
            ["clone", "clone3", "fork", "vfork"].includes(call.name) &&
            ok &&
            Number(call.result) > 0
        ) {
            const child = Number(call.result);
            if (call.args.includes("CLONE_THREAD")) {
                threadOf.set(child, pid);
            } else {
                builder.fork(pid, child);
                parentOf.set(child, pid);
                cwds.set(child, cwds.get(pid) ?? "/");
            }
        } else if (call.name === "chdir" && call.result === "0") {
            const [target] = straceStrings(call.args);
            if (typeof target === "string") {
                cwds.set(pid, path.posix.resolve(cwds.get(pid) ?? "/", target));
            }
        } else if (call.name === "fchdir" && call.result === "0") {
            const target = /<(.*)>/.exec(call.args)?.[1];
            if (target !== undefined) {
                cwds.set(pid, target);
            }
        } else if ((call.name === "execve" || call.name === "execveat") && call.result === "0") {
            const [exe, args] = straceStrings(call.args);
            if (typeof exe !== "string" || !Array.isArray(args)) {
                continue;
            }
            const dir = cwds.get(pid) ?? "/";
            builder.exec(
                pid,
                parentOf.get(pid) ?? -1,
                exe.startsWith("/") ? exe : path.posix.resolve(dir, exe),
                args,
                dir,
            );
        }
    }
    return builder;
}

export class StraceRecorder implements Recorder {
    readonly name = "strace";
    private readonly dir: string;
    private readonly cwd: string;

    constructor(work: string, cwd: string) {
        this.dir = path.join(work, "strace");
        this.cwd = cwd;
    }

    wrap(command: string[]): string[] {
        return [
            "strace",
            "-ff",
            "-ttt",
            "-qq",
            "-e",
            "signal=none",
            "-s",
            "1000000",
            "-y",
            "--seccomp-bpf",
            "-e",
            "trace=execve,execveat,chdir,fchdir,clone,clone3,fork,vfork",
            "-o",
            path.join(this.dir, "t"),
            ...command,
        ];
    }

    start(): void {
        fs.rmSync(this.dir, { recursive: true, force: true });
        fs.mkdirSync(this.dir, { recursive: true });
    }

    stop(): Promise<Recording> {
        return Promise.resolve({ images: parseStrace(this.dir, this.cwd).build(), lost: 0 });
    }
}
