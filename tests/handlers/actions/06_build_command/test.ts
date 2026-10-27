import { test } from "@catter/tools/testing";

// The handler acts on the build command itself, as on any other: dropped, it
// does not run and the build exits 0; modified, the command it returns runs.
test("dropped", async (t) => {
    await t.cli(["run", "--handler", "python handler.py", "--", "node", "child.mjs", "drop-me"]);
});

test("modified", async (t) => {
    await t.cli(["run", "--handler", "python handler.py", "--", "node", "child.mjs", "modify-me"]);
});
