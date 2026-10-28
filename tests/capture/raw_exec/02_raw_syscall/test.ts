import { test } from "@catter/tools/testing";

// A program the hook loads into starts programs with the exec system call
// itself, in a child of its own and in its own place: each program reports
// itself, as a child of the one that started it.
test(
    "linux",
    { except: { darwin: "the hook of macOS sees every exec", win32: "the POSIX functions" } },
    async (t) => {
        await t.setup("cc -o launch launch.c");
        await t.cli([
            "tree",
            "-w",
            "1000",
            "--",
            "./launch",
            "fork",
            process.execPath,
            "child.mjs",
        ]);
        await t.cli([
            "tree",
            "-w",
            "1000",
            "--",
            "./launch",
            "exec",
            process.execPath,
            "child.mjs",
        ]);
    },
);
