import { test } from "@catter/tools/testing";

// A built-in and a handler: the built-in, first, sees every command the
// build started, and the handler decides after it.
test(async (t) => {
    await t.cli(["tree", "--handler", "python decider.py", "--", "node", "build.mjs"]);
});
