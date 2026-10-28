import { test } from "@catter/tools/testing";

// A process of the build that is stopped stays stopped until it is
// continued, as job control has it, though catter traces it.
test(
    { except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" } },
    async (t) => {
        await t.setup("cc -o stop stop.c");
        await t.run("./stop");
        await t.cli("tree --capture ptrace -w 1000 -- ./stop");
    },
);
