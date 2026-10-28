import { test } from "@catter/tools/testing";

// A program that is several threads starts another from one that is not its
// main thread: the child is the program's, and a program run in its place is
// the program's process still.
test(
    { except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" } },
    async (t) => {
        await t.setup("cc -pthread -o spawn spawn.c");
        await t.cli("tree --capture ptrace -w 1000 -- ./spawn spawn");
        await t.cli("tree --capture ptrace -w 1000 -- ./spawn exec");
    },
);
