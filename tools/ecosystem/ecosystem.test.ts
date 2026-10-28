import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { afterEach, describe, expect, test } from "vitest";
import { fileKind } from "./checks/artifacts.ts";
import { checkHint } from "./checks/hint.ts";
import { checkRecorder, sameArgs, type Entry } from "./checks/recorder.ts";
import { syntaxOnly } from "./checks/syntax.ts";
import { expandResponseFiles, isCompiler, isWrapped, shellSplit } from "./command.ts";
import { isKnown, Places } from "./difference.ts";
import { listJobs, projectNames, readProject, type Known } from "./project.ts";
import { parseAudit } from "./record/audit.ts";
import { ImageBuilder } from "./record/images.ts";
import { parseStrace } from "./record/strace.ts";
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
        expect(isKnown(known, difference, { build: "b", backend: "hook", platform: "linux" })).toBe(
            true,
        );
        expect(
            isKnown(known, difference, { build: "b", backend: "wrapper", platform: "linux" }),
        ).toBe(false);
        expect(
            isKnown(
                known,
                { ...difference, detail: "<tree>/a.o" },
                { build: "b", backend: "hook", platform: "linux" },
            ),
        ).toBe(false);
    });
});

describe("images", () => {
    test("an exec logged before the fork that made its process keeps its parent", () => {
        const builder = new ImageBuilder();
        const make = builder.exec(10, 1, "/usr/bin/make", ["make"], "/w");
        const cc = builder.exec(11, 10, "/usr/bin/cc", ["cc", "-c", "a.c"], "/w");
        builder.fork(10, 11);
        builder.fork(11, 12);
        const as = builder.exec(12, 11, "/usr/bin/as", ["as"], "/w");
        expect(cc.parent).toBe(make.id);
        expect(as.parent).toBe(cc.id);
    });

    test("a double fork keeps the daemon under what forked it", () => {
        const builder = new ImageBuilder();
        const client = builder.exec(10, 1, "/usr/bin/bazel", ["bazel", "build"], "/w");
        builder.fork(10, 11);
        builder.fork(11, 12);
        // The middle process is gone: the daemon's parent is init by now.
        const server = builder.exec(12, 1, "/usr/bin/java", ["java"], "/w");
        expect(server.parent).toBe(client.id);
    });
});

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
        const images = parseAudit(log, key).images;
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
        const images = parseStrace(dir, "/w").images;
        expect(images.map((image) => [image.exe, image.args, image.cwd, image.parent])).toEqual([
            ["/usr/bin/make", ["make", "-C", "sub"], "/w", null],
            ["/usr/bin/cc", ["cc", "-c", "aé.c"], "/w/sub", 0],
        ]);
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
        return { images: builder.images, lost: 0 };
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

    test("under the wrapper, what a compiler driver starts is the driver's", () => {
        const result = checkRecorder({
            ...base,
            backend: "wrapper",
            recording: recording([
                [3, 1, "/usr/bin/make", ["make"]],
                [4, 3, "/opt/catter/bin/catter-proxy", ["cc", "-c", "a.c"]],
                [5, 4, "/usr/bin/cc", ["cc", "-c", "a.c"]],
                [6, 5, "/usr/lib/gcc/as", ["as"]],
                [7, 3, "/usr/bin/ar", ["/usr/bin/ar", "rcs", "liba.a"]],
            ]),
            session: session([{ args: ["cc", "-c", "a.c"] }]),
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
        expect(fileKind(elf(1), "a.o")).toBe("object");
        expect(fileKind(elf(2), "a")).toBe("linked");
        expect(fileKind(elf(3), "liba.so")).toBe("linked");
        expect(fileKind(Buffer.from("!<arch>\nfoo"), "liba.a")).toBe("archive");
        const macho = Buffer.alloc(32);
        macho.writeUInt32LE(0xfeedfacf, 0);
        macho.writeUInt32LE(1, 12);
        expect(fileKind(macho, "a.o")).toBe("object");
        expect(fileKind(Buffer.from("BC\xc0\xde....", "latin1"), "a.o")).toBe("object");
        expect(fileKind(Buffer.from("int main() {}"), "a.c")).toBeUndefined();
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
