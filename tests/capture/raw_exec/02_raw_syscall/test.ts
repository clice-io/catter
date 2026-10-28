import { test } from "@catter/tools/testing";

// A program the hook loads into starts programs with the exec system call
// itself, in a child of its own, in its own place, and itself again as
// /proc/self/exe names it: each program reports itself, as a child of the
// one that started it.
test(
    "linux",
    { except: { darwin: "the hook of macOS sees every exec", win32: "the POSIX functions" } },
    async (t) => {
        await t.setup("cc -o launch launch.c");
        for (const mode of ["fork", "exec", "self"]) {
            await t.cli([
                "tree",
                "-w",
                "1000",
                "--",
                "./launch",
                mode,
                process.execPath,
                "child.mjs",
            ]);
        }
    },
);
