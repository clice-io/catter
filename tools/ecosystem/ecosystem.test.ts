import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { afterEach, describe, expect, test } from "vitest";
import { fileKind } from "./checks/artifacts.ts";
import { checkHint } from "./checks/hint.ts";
import { checkRecorder, sameArgs, type Entry } from "./checks/recorder.ts";
import { syntaxOnly } from "./checks/syntax.ts";
import {
    expandResponseFiles,
    isCompiler,
    isWrapped,
    shellSplit,
    sourceOperands,
} from "./command.ts";
import { isKnown, Places } from "./difference.ts";
import type { JobResult } from "./job.ts";
import { listJobs, projectNames, readProject, type Job, type Known } from "./project.ts";
import { parseAudit } from "./record/audit.ts";
import { ImageBuilder } from "./record/images.ts";
import { parseStrace } from "./record/strace.ts";
import { isRed, roundReport } from "./report.ts";
import type { Command, Session } from "./session.ts";

const dirs: string[] = [];
function tempDir(): string {
    const dir = fs.realpathSync(fs.mkdtempSync(path.join(os.tmpdir(), "ecosystem-test-")));
    dirs.push(dir);
    return dir;
}
afterEach(() => {
    for (const dir of dirs.splice(0)) {
        fs.rmSync(dir, { recursive: true, force: true });
    }
});

describe("projects", () => {
    test("every project.toml reads, and lists its jobs", () => {
        const names = projectNames();
        expect(names.length).toBeGreaterThan(0);
        for (const name of names) {
            expect(() => readProject(name), name).not.toThrow();
        }
        const jobs = listJobs();
        expect(new Set(jobs.map((job) => job.artifact)).size).toBe(jobs.length);
    });

    test("a known difference is as narrow as its fields", () => {
        const known: Known = {
            why: "w",
            check: "artifacts",
            kind: "object-without-entry",
            backend: "hook",
            detail: /\.lo$/,
        };
        const difference = {
            check: "artifacts" as const,
            kind: "object-without-entry",
            detail: "no entry has the output <tree>/a.lo",
        };
        expect(isKnown(known, difference, { build: "b", backend: "hook" })).toBe(true);
        expect(isKnown(known, difference, { build: "b", backend: "wrapper" })).toBe(false);
        expect(
            isKnown(
                known,
                { ...difference, detail: "<tree>/a.o" },
                { build: "b", backend: "hook" },
            ),
        ).toBe(false);
    });
});

describe("images", () => {
    const parents = (builder: ImageBuilder) =>
        builder.build().map((image) => [image.args[0], image.parent]);

    test("an exec logged before the forks that made its process keeps its parent", () => {
        const builder = new ImageBuilder();
        builder.exec(10, 1, "/usr/bin/make", ["make"], "/w");
        // A subshell of make forks a compiler, which execs before either
        // fork is logged.
        builder.fork(11, 12);
        builder.exec(12, 11, "/usr/bin/cc", ["cc", "-c", "a.c"], "/w");
        builder.fork(10, 11);
        builder.fork(12, 13);
        builder.exec(13, 12, "/usr/bin/as", ["as"], "/w");
        expect(parents(builder)).toEqual([
            ["make", null],
            ["cc", 0],
            ["as", 1],
        ]);
    });

    test("a child runs what its parent ran when it forked, not what it runs next", () => {
        const builder = new ImageBuilder();
        builder.exec(10, 1, "/bin/sh", ["sh"], "/w");
        builder.fork(10, 11);
        builder.exec(10, 1, "/usr/bin/ar", ["ar"], "/w");
        builder.exec(11, 10, "/usr/bin/cc", ["cc"], "/w");
        expect(parents(builder)).toEqual([
            ["sh", null],
            ["ar", 0],
            ["cc", 0],
        ]);
    });

    test("a double fork keeps the daemon under what forked it", () => {
        const builder = new ImageBuilder();
        builder.exec(10, 1, "/usr/bin/bazel", ["bazel", "build"], "/w");
        builder.fork(10, 11);
        builder.fork(11, 12);
        // The middle process is gone: the daemon's parent is init by now.
        builder.exec(12, 1, "/usr/bin/java", ["java"], "/w");
        builder.exec(12, 1, "/usr/bin/java", ["java", "again"], "/w");
        expect(parents(builder)).toEqual([
            ["bazel", null],
            ["java", 0],
            ["java", 1],
        ]);
    });
});

// A daemon's double fork: the server execs once its parent is gone.
const daemonLog = [
    `type=SYSCALL msg=audit(1.0:1): arch=c000003e syscall=59 success=yes exit=0 ppid=1 pid=10 exe="/usr/bin/bazel" key="k"`,
    `type=EXECVE msg=audit(1.0:1): argc=2 a0="bazel" a1="build"`,
    `type=SYSCALL msg=audit(1.0:2): arch=c000003e syscall=57 success=yes exit=11 ppid=1 pid=10 key="k"`,
    `type=SYSCALL msg=audit(1.0:3): arch=c000003e syscall=435 success=yes exit=12 ppid=10 pid=11 key="k"`,
    `type=SYSCALL msg=audit(1.0:4): arch=c000003e syscall=59 success=yes exit=0 ppid=1 pid=12 exe="/usr/bin/java" key="k"`,
    `type=EXECVE msg=audit(1.0:4): argc=1 a0="java"`,
].join("\n");

describe("auditd", () => {
    const key = "k";
    const hex = (text: string) => Buffer.from(text).toString("hex").toUpperCase();
    const log = [
        `type=SYSCALL msg=audit(1.0:1): arch=c000003e syscall=56 success=yes exit=101 ppid=1 pid=100 key="k"\x1dARCH=x86_64 SYSCALL=clone`,
        `type=SYSCALL msg=audit(1.0:2): arch=c000003e syscall=59 success=yes exit=0 ppid=100 pid=101 exe="/usr/bin/gcc" key="k"`,
        `type=EXECVE msg=audit(1.0:2): argc=4 a0="gcc" a1=${hex("-DX=a b")} a2_len=6 a2[0]=${hex("-c")} a2[1]=${hex("x.c")} a3="-o"`,
        `type=CWD msg=audit(1.0:2): cwd=${hex("/w/é dir")}`,
        `type=SYSCALL msg=audit(1.0:3): arch=c000003e syscall=59 success=yes exit=0 ppid=1 pid=200 exe="/bin/ls" key="other"`,
        `type=SYSCALL msg=audit(1.0:4): arch=c000003e syscall=59 success=no exit=-2 ppid=100 pid=102 exe="/nope" key="k"`,
    ].join("\n");

    test("execs with their arguments and directory, under the run's key alone", () => {
        const images = parseAudit(log, key).build();
        expect(images).toEqual([
            {
                id: 0,
                parent: null,
                pid: 101,
                exe: "/usr/bin/gcc",
                args: ["gcc", "-DX=a b", "-cx.c", "-o"],
                cwd: "/w/é dir",
            },
        ]);
    });
});

describe("auditd forks", () => {
    test("a fork the log records takes a daemon's exec to what forked it", () => {
        const images = parseAudit(daemonLog, "k").build();
        expect(images.map((image) => [image.args, image.parent])).toEqual([
            [["bazel", "build"], null],
            [["java"], 0],
        ]);
    });
});

describe("strace", () => {
    test("each process's directory follows its chdirs and its parent's", () => {
        const dir = tempDir();
        fs.writeFileSync(
            path.join(dir, "t.10"),
            [
                `1.0 execve("/usr/bin/make", ["make", "-C", "sub"], 0x0 /* 3 vars */) = 0`,
                `1.1 chdir("sub") = 0`,
                `1.2 clone(child_stack=NULL, flags=CLONE_CHILD_SETTID|SIGCHLD) = 11`,
            ].join("\n"),
        );
        fs.writeFileSync(
            path.join(dir, "t.11"),
            `1.3 execve("/usr/bin/cc", ["cc", "-c", "a\\303\\251.c"], 0x0 /* 3 vars */) = 0\n`,
        );
        const images = parseStrace(dir, "/w").build();
        expect(images.map((image) => [image.exe, image.args, image.cwd, image.parent])).toEqual([
            ["/usr/bin/make", ["make", "-C", "sub"], "/w", null],
            ["/usr/bin/cc", ["cc", "-c", "aé.c"], "/w/sub", 0],
        ]);
    });
});

describe("strace through a symlink", () => {
    test("a directory reads as the kernel names it", () => {
        const dir = tempDir();
        fs.mkdirSync(path.join(dir, "real"));
        fs.symlinkSync(path.join(dir, "real"), path.join(dir, "link"));
        const traces = path.join(dir, "traces");
        fs.mkdirSync(traces);
        fs.writeFileSync(
            path.join(traces, "t.10"),
            [
                `1.0 execve("/usr/bin/make", ["make"], 0x0 /* 3 vars */) = 0`,
                `1.1 chdir("link") = 0`,
                `1.2 execve("/usr/bin/cc", ["cc"], 0x0 /* 3 vars */) = 0`,
            ].join("\n"),
        );
        expect(
            parseStrace(traces, dir)
                .build()
                .map((image) => image.cwd),
        ).toEqual([dir, path.join(dir, "real")]);
    });
});

describe("the recorder check", () => {
    const catter = "/opt/catter/bin/catter";
    function session(commands: Partial<Command>[]): Session {
        return {
            commands: commands.map((command, i) => ({
                id: i + 1,
                parent: null,
                cwd: "/w",
                exe: "",
                args: [],
                env: [],
                analysis: null,
                ...command,
            })),
            errors: [],
            finish: 0,
        };
    }
    function recording(execs: [number, number, string, string[]][]) {
        const builder = new ImageBuilder();
        builder.exec(1, 0, catter, ["catter"], "/w");
        for (const [pid, ppid, exe, args] of execs) {
            builder.fork(ppid, pid);
            builder.exec(pid, ppid, exe, args, "/w");
        }
        return { images: builder.build(), lost: 0 };
    }
    const places = new Places({ tree: "/w" });
    const base = {
        entries: [] as Entry[],
        catter,
        own: ["/opt/catter/bin"],
        handler: "/repo/handler.ts",
        places,
    };

    test("under the hook, catter's own processes are dropped and the trees match", () => {
        const result = checkRecorder({
            ...base,
            backend: "hook",
            recording: recording([
                [2, 1, "/usr/bin/node", ["node", "/repo/handler.ts"]],
                [3, 1, "/opt/catter/bin/catter-proxy", ["catter-proxy"]],
                [4, 3, "/usr/bin/make", ["make"]],
                [5, 4, "/opt/catter/bin/catter-proxy", ["catter-proxy"]],
                [6, 5, "/usr/bin/cc", ["cc", "-c", "a.c"]],
                [7, 4, "/usr/bin/true", ["true"]],
            ]),
            session: session([
                { args: ["make"] },
                { parent: 1, args: ["cc", "-c", "a.c"] },
                { parent: 2, args: ["cc1"] },
            ]),
        });
        expect(result.differences.map((each) => [each.kind, each.detail])).toEqual([
            ["extra", "catter reported cc1 [in <tree>], which the recorder never saw there"],
            ["missed", "catter missed true [in <tree>]"],
        ]);
        expect(result.stats).toMatchObject({ kept: 3, commands: 3, matched: 2 });
    });

    test("a compile run twice alike is one entry", () => {
        const dir = tempDir();
        fs.writeFileSync(path.join(dir, "a.C"), "");
        const builder = new ImageBuilder();
        builder.exec(1, 0, catter, ["catter"], dir);
        for (const pid of [2, 3]) {
            builder.fork(1, pid);
            builder.exec(pid, 1, "/usr/bin/cc", ["cc", "-c", "a.C"], dir);
        }
        const commands = [1, 2].map((id) => ({
            id,
            parent: null,
            cwd: dir,
            exe: "",
            args: ["cc", "-c", "a.C"],
            env: [],
            analysis: null,
        }));
        const result = checkRecorder({
            ...base,
            places: new Places({ tree: dir }),
            backend: "hook",
            recording: { images: builder.build(), lost: 0 },
            session: { commands, errors: [], finish: 0 },
            entries: [{ directory: dir, file: "a.C", arguments: ["cc", "-c", "a.C"] }],
        });
        expect(result.differences).toEqual([]);
        expect(result.stats.compiles).toBe(2);
    });

    test("a file an option names is no source of the compile", () => {
        const dir = tempDir();
        fs.writeFileSync(path.join(dir, "a.c"), "");
        fs.writeFileSync(path.join(dir, "prefix.c"), "");
        expect([
            ...sourceOperands(["gcc", "-include", "prefix.c", "-c", "a.c", "-o", "a.o"], dir),
        ]).toEqual([4]);
    });

    test("a run that only lists the includes is no compile", () => {
        const dir = tempDir();
        fs.writeFileSync(path.join(dir, "a.c"), "");
        const builder = new ImageBuilder();
        builder.exec(1, 0, catter, ["catter"], dir);
        builder.fork(1, 2);
        builder.exec(2, 1, "/usr/bin/gcc", ["gcc", "-MM", "a.c"], dir);
        builder.fork(1, 3);
        builder.exec(3, 1, "/usr/bin/gcc", ["gcc", "-c", "a.c", "-o", "a.o"], dir);
        const result = checkRecorder({
            ...base,
            places: new Places({ tree: dir }),
            backend: "hook",
            recording: { images: builder.build(), lost: 0 },
            session: {
                commands: [
                    {
                        id: 1,
                        parent: null,
                        cwd: dir,
                        exe: "",
                        args: ["gcc", "-MM", "a.c"],
                        env: [],
                        analysis: null,
                    },
                    {
                        id: 2,
                        parent: null,
                        cwd: dir,
                        exe: "",
                        args: ["gcc", "-c", "a.c", "-o", "a.o"],
                        env: [],
                        analysis: null,
                    },
                ],
                errors: [],
                finish: 0,
            },
            entries: [
                {
                    directory: dir,
                    file: "a.c",
                    output: "a.o",
                    arguments: ["gcc", "-c", "a.c", "-o", "a.o"],
                },
            ],
        });
        expect(result.differences).toEqual([]);
    });

    test("catter's own programs are dropped by name, not by the directory they sit in", () => {
        const result = checkRecorder({
            ...base,
            catter: "/usr/bin/catter",
            own: ["/home/catter"],
            backend: "hook",
            recording: (() => {
                const builder = new ImageBuilder();
                builder.exec(1, 0, "/usr/bin/catter", ["catter"], "/w");
                builder.fork(1, 2);
                builder.exec(2, 1, "/usr/bin/catter-proxy", ["catter-proxy"], "/w");
                builder.fork(2, 3);
                builder.exec(3, 2, "/usr/bin/make", ["make"], "/w");
                return { images: builder.build(), lost: 0 };
            })(),
            session: session([{ args: ["make"] }]),
        });
        expect(result.differences).toEqual([]);
        expect(result.stats).toMatchObject({ kept: 1, matched: 1 });
    });

    test("a script runs behind its interpreter", () => {
        expect(
            sameArgs(
                ["/env/bin/python3.14", "/env/bin/meson", "compile", "-C", "b"],
                ["meson", "compile", "-C", "b"],
            ),
        ).toBe(true);
        expect(sameArgs(["/bin/sh", "-e", "./configure", "--x"], ["./configure", "--x"])).toBe(
            true,
        );
        expect(sameArgs(["/bin/sh", "./other", "--x"], ["./configure", "--x"])).toBe(false);
        expect(sameArgs(["cc", "-c", "a.c"], ["cc", "-c", "b.c"])).toBe(false);
    });

    test("under the wrapper, what a compiler driver starts is the driver's, nothing else is", () => {
        const result = checkRecorder({
            ...base,
            backend: "wrapper",
            recording: recording([
                [2, 1, "/opt/catter/bin/catter-launch", ["catter-launch", "make"]],
                [3, 2, "/usr/bin/make", ["make"]],
                [4, 3, "/opt/catter/bin/catter-proxy", ["cc", "a.o", "-o", "a"]],
                [5, 4, "/usr/bin/cc", ["cc", "a.o", "-o", "a"]],
                [6, 5, "/usr/bin/ld", ["/usr/bin/ld", "a.o", "-o", "a"]],
                [7, 3, "/usr/bin/ar", ["/usr/bin/ar", "rcs", "liba.a"]],
            ]),
            session: session([{ args: ["make"] }, { parent: 1, args: ["cc", "a.o", "-o", "a"] }]),
        });
        expect(result.differences.map((each) => [each.kind, each.detail])).toEqual([
            ["missed", "catter missed /usr/bin/ar rcs liba.a [in <tree>]"],
        ]);
    });
});

describe("artifacts", () => {
    function elf(type: number): Buffer {
        const head = Buffer.alloc(64);
        head.write("\x7fELF", 0, "latin1");
        head[5] = 1;
        head.writeUInt16LE(type, 16);
        return head;
    }

    test("files are told apart by their magic", () => {
        expect(fileKind(elf(1))).toBe("object");
        expect(fileKind(elf(2))).toBe("linked");
        expect(fileKind(elf(3))).toBe("linked");
        expect(fileKind(Buffer.from("!<arch>\nfoo"))).toBe("archive");
        expect(fileKind(Buffer.from("BC\xc0\xde....", "latin1"))).toBe("object");
        expect(fileKind(Buffer.from("int main() {}"))).toBeUndefined();
    });
});

describe("reports", () => {
    test("a job that stopped, or left no results, is red in the round", () => {
        const stopped: JobResult = {
            job: "a/make",
            platform: "linux",
            catter: "c",
            backends: [],
            unseen: [],
            error: "no source",
        };
        const jobs: Job[] = [
            { name: "a/make", platform: "linux", packages: "", artifact: "a-make-linux" },
            { name: "b/make", platform: "linux", packages: "", artifact: "b-make-linux" },
        ];
        expect(isRed(stopped)).toBe(true);
        const round = roundReport([stopped], jobs);
        expect(round).toContain("| 🔴 b/make | linux | | no results | |");
        expect(round).toContain("| 🔴 a/make | linux |  | 0, stopped | 0 |");
        expect(round).toContain("The job stopped: no source");
    });
});

describe("command lines", () => {
    test("a database's command splits as a shell splits it", () => {
        expect(shellSplit(`cc -DA=\\"b\\" "-DC=d e" 'f\\g' -c x.c`)).toEqual([
            "cc",
            '-DA="b"',
            "-DC=d e",
            "f\\g",
            "-c",
            "x.c",
        ]);
    });

    test("compilers and the programs the wrapper stands in for, by name", () => {
        expect(isCompiler("/usr/bin/x86_64-linux-gnu-gcc-13")).toBe(true);
        expect(isCompiler("/usr/libexec/gcc/x86_64-linux-gnu/13/cc1plus")).toBe(false);
        expect(isWrapped("/usr/bin/ld.gold")).toBe(true);
        expect(isWrapped("/usr/bin/make")).toBe(false);
    });

    test("response files are read in", () => {
        const dir = tempDir();
        fs.writeFileSync(path.join(dir, "args.rsp"), `-DX "-I/a b"\n-c`);
        expect(expandResponseFiles(["cc", "@args.rsp", "x.c", "@missing"], dir)).toEqual([
            "cc",
            "-DX",
            "-I/a b",
            "-c",
            "x.c",
            "@missing",
        ]);
    });

    test("an entry checks its code without writing its object or dependencies", () => {
        expect(
            syntaxOnly([
                "c++",
                "-DX",
                "-MD",
                "-MT",
                "a.o",
                "-MF",
                "a.o.d",
                "-o",
                "a.o",
                "-c",
                "a.cc",
            ]),
        ).toEqual(["c++", "-DX", "a.cc", "-fsyntax-only"]);
        expect(syntaxOnly(["cl", "/c", "a.cc"])).toBeUndefined();
    });
});

describe("the hint check", () => {
    test("entries pair by directory, source and output; dependency files and shims aside", () => {
        const dir = tempDir();
        const hint = path.join(dir, "compile_commands.json");
        fs.writeFileSync(
            hint,
            JSON.stringify([
                { directory: dir, file: "a.c", output: "a.o", command: "cc -DX -c a.c -o a.o" },
                {
                    directory: dir,
                    file: "d.c",
                    output: "d.o",
                    command: "/home/catter/shims-1/1/cc -c d.c -o d.o",
                },
                { directory: dir, file: "b.c", output: "b.o", command: "cc -c b.c -o b.o" },
            ]),
        );
        const entries: Entry[] = [
            {
                directory: dir,
                file: "a.c",
                output: "a.o",
                arguments: ["cc", "-DY", "-c", "a.c", "-o", "a.o"],
            },
            {
                directory: dir,
                file: "c.c",
                output: "c.o",
                arguments: ["cc", "-c", "c.c", "-o", "c.o"],
            },
            {
                directory: dir,
                file: "d.c",
                output: "d.o",
                arguments: ["/usr/bin/cc", "-MD", "-MF", "d.o.d", "-c", "d.c", "-o", "d.o"],
            },
        ];
        const found = checkHint({
            entries,
            hint,
            home: "/home/catter",
            places: new Places({ tree: dir }),
        });
        expect(found.map((each) => [each.kind, each.detail])).toEqual([
            [
                "arguments",
                "<tree>/a.c: at argument 1: catter has cc -DY -c a.c, the build's cc -DX -c a.c",
            ],
            ["catter-only", "the build's database has no entry of <tree>/c.c to c.o"],
            ["build-only", "catter's database has no entry of <tree>/b.c to b.o"],
        ]);
    });
});
