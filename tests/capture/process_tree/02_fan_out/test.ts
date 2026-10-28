import { test } from "@catter/tools/testing";

// The case's handler.py counts what catter saw of build.mjs's programs.
test("hook", async (t) => {
    await t.run("node build.mjs");
    await t.cli(["run", "--handler", "python handler.py", "--", "node", "build.mjs"]);
});

const linuxOnly = {
    except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" },
};

test("ptrace", linuxOnly, async (t) => {
    await t.cli([
        "run",
        "--capture",
        "ptrace",
        "--handler",
        "python handler.py observe",
        "--",
        "node",
        "build.mjs",
    ]);
});
