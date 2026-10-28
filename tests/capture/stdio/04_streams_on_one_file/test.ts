import { test } from "@catter/tools/testing";

// run.mjs starts catter itself, on streams that are no pipes of the runner:
// what the build's programs find there tells whether they are catter's own.
test("hook", async (t) => {
    await t.run("node run.mjs");
});

const linuxOnly = {
    except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" },
};

test("ptrace", linuxOnly, async (t) => {
    await t.run("node run.mjs --capture ptrace");
});
