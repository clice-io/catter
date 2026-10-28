import { test } from "@catter/tools/testing";

// The program a build step runs in a directory it removed cannot be captured: it is left out of
// the trace, under ptrace too, which still sees it exit.
test(
    "hook",
    { except: { win32: "Windows does not let a directory a process runs in be removed" } },
    async (t) => {
        await t.cli("profile -q -- node build.mjs");
        t.show("trace.json", t.disk.trace());
    },
);

test(
    "ptrace",
    { except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" } },
    async (t) => {
        await t.cli("profile -q --capture ptrace -- node build.mjs");
        t.show("trace.json", t.disk.trace());
    },
);
