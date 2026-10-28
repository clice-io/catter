import { test } from "@catter/tools/testing";

// The handler acts on programs a static program starts, which report
// themselves late: one runs changed, one does not run and ends with 0.
test(
    "linux",
    { except: { darwin: "macOS has no static programs", win32: "the POSIX functions" } },
    async (t) => {
        await t.setup("cc -static -o launch launch.c");
        for (const tag of ["act-on-me", "drop-me", "leave-me"]) {
            await t.cli([
                "run",
                "--handler",
                "python handler.py",
                "--",
                "./launch",
                "fork",
                "node",
                "child.mjs",
                tag,
            ]);
        }
    },
);
