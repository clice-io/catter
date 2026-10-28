import { test } from "@catter/tools/testing";

// catter is stopped while the build runs: the build stops as it would be
// stopped itself, and catter ends as the build did. Killed, catter takes the
// build with it.
test("terminated", async (t) => {
    await t.run("node stop.mjs SIGTERM node build.mjs");
    await t.run("node stop.mjs SIGTERM catter tree -w 1000 -- node build.mjs");
});

test("killed", async (t) => {
    await t.run("node stop.mjs SIGKILL node build.mjs");
    await t.run("node stop.mjs SIGKILL catter tree -w 1000 -- node build.mjs");
});

const linuxOnly = {
    except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" },
};

test("ptrace terminated", linuxOnly, async (t) => {
    await t.run("node stop.mjs SIGTERM catter tree --capture ptrace -w 1000 -- node build.mjs");
});

test("ptrace killed", linuxOnly, async (t) => {
    await t.run("node stop.mjs SIGKILL catter tree --capture ptrace -w 1000 -- node build.mjs");
});
