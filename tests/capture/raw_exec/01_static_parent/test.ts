import { test } from "@catter/tools/testing";

// A static program starts programs with exec calls the hook never sees, in a
// child of its own and in its own place: each program the hook then loads
// into reports itself, as a child of the static one.
test(
    "linux",
    { except: { darwin: "macOS has no static programs", win32: "the POSIX functions" } },
    async (t) => {
        await t.setup("cc -static -o launch launch.c");
        await t.cli("tree -w 1000 -- ./launch fork node child.mjs");
        await t.cli("tree -w 1000 -- ./launch exec node child.mjs");
        await t.cli("compile-commands -o compile_commands.json -- ./launch fork cc -c main.c");
        t.show("compile_commands.json", t.disk.cdb());
    },
);
