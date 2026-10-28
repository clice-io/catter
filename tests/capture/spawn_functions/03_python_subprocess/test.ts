import { test } from "@catter/tools/testing";

test("hook", async (t) => {
    await t.run("python build.py");
    await t.cli("tree -w 1000 -- python build.py");
});

const linuxOnly = {
    except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" },
};

test("ptrace", linuxOnly, async (t) => {
    await t.cli("tree --capture ptrace -w 1000 -- python build.py");
});
