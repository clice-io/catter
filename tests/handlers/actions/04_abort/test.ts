import { test } from "@catter/tools/testing";

// The case's handler.py aborts the build at one command of build.mjs: that
// command and every later one fail without running. A replayed build stops
// there.
test("inject", { errors: ["aborted the build at command 2"] }, async (t) => {
    await t.cli(["run", "--handler", "python handler.py", "--", "node", "build.mjs"]);
});

test("replay", { errors: ["aborted the build at command 2"] }, async (t) => {
    await t.cli(["run", "--handler", "python handler.py", "--from", "events.json"]);
});
