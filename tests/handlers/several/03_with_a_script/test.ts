import { test } from "@catter/tools/testing";

// A builtin script and a handler: the script, first, sees every command the
// build started, and the handler decides after it.
test(async (t) => {
    await t.cli(["--handler", "python decider.py", "script::cmd-tree", "--", "node", "build.mjs"]);
});
