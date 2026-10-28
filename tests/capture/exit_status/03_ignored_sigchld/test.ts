import { test } from "@catter/tools/testing";

test("hook", { except: { win32: "Windows has no SIGCHLD" } }, async (t) => {
    await t.run("node build.mjs");
    await t.cli("tree -w 1000 -- node build.mjs");
});

const linuxOnly = {
    except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" },
};

test("ptrace", linuxOnly, async (t) => {
    await t.cli("tree --capture ptrace -w 1000 -- node build.mjs");
});
