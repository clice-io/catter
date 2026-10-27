import { test } from "@catter/tools/testing";

// The case's handler.py prints what each callback receives: under the hook,
// and under replay, with a command catter could not capture.
test("hook", async (t) => {
    await t.cli(["run", "--handler", "python handler.py", "--", "node", "build.mjs"]);
});

test("replay", async (t) => {
    await t.cli(["run", "--handler", "python handler.py", "--from", "events.json"]);
});
