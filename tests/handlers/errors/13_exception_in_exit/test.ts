import { test } from "@catter/tools/testing";

// An exception in a handler's on_exit, which answers nothing, stops the
// handler: the commands that start after it fail without running.
test({ errors: ["stopped before it answered command 3"] }, async (t) => {
    await t.cli(["run", "--handler", "python handler.py", "--", "node", "build.mjs"]);
});
