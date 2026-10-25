import { test } from "@catter/tools/testing";

// A handler that exits in the middle of the build ends the session: the
// command it did not answer and every later one fail without running.
test({ errors: ["stopped before it answered command 3"] }, async (t) => {
    await t.cli(["--handler", "python handler.py", "--", "node", "build.mjs"]);
});
