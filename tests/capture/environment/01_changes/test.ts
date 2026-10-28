import { test } from "@catter/tools/testing";

// The build itself runs with a variable the program it starts must not see.
test("hook", async (t) => {
    const env = { CATTER_PROBE_REMOVED: "the build's own" };
    await t.run("node build.mjs", { env });
    await t.cli("tree -w 1000 -- node build.mjs", { env });
});

const linuxOnly = {
    except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" },
};

test("ptrace", linuxOnly, async (t) => {
    const env = { CATTER_PROBE_REMOVED: "the build's own" };
    await t.cli("tree --capture ptrace -w 1000 -- node build.mjs", { env });
});
