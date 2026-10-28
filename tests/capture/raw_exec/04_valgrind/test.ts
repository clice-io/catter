import { test } from "@catter/tools/testing";

// valgrind runs the program inside its own process: the program stays there,
// where memcheck finds its error and valgrind exits with the code it is told.
test(
    "linux",
    { except: { darwin: "valgrind runs on Linux", win32: "valgrind runs on Linux" } },
    async (t) => {
        await t.setup("cc -g -O0 -o bad bad.c");
        await t.cli("tree -w 1000 -- valgrind -q --error-exitcode=9 --log-file=/dev/null ./bad");
    },
);
