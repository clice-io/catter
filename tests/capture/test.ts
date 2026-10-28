import { test } from "@catter/tools/testing";

// A case's build.mjs starts programs the way a build tool does and prints
// what they received: run directly, then under catter, which shows what it
// captured.
test("hook", async (t) => {
    await t.run("node build.mjs");
    await t.cli("tree -w 1000 -- node build.mjs");
});

const linuxOnly = {
    except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" },
};

test("ptrace", linuxOnly, async (t) => {
    await t.cli("tree --capture ptrace -w 1000 -- node build.mjs");
});
