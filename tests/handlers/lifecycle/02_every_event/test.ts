import { test } from "@catter/tools/testing";

// The case's handler.py prints what each callback receives: under inject,
// and under replay, with a command catter could not capture.
test("inject", async (t) => {
    await t.cli(["--handler", "python handler.py", "--", "node", "build.mjs"]);
});

test("replay", async (t) => {
    await t.cli(["-m", "replay", "--handler", "python handler.py", "--", "events.json"]);
});
