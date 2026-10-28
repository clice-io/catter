import { test } from "@catter/tools/testing";

// valgrind runs the program inside its own process, where it stays: no
// command of its own, as a restart of it would show. The tool that adds
// nothing needs none of the debug information of the C library memcheck
// needs.
test(
    "linux",
    { except: { darwin: "valgrind runs on Linux", win32: "valgrind runs on Linux" } },
    async (t) => {
        await t.setup("cc -o main main.c");
        await t.cli("tree -w 1000 -- valgrind --tool=none -q ./main");
    },
);
