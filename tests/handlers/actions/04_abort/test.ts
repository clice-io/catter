import { test } from "@catter/tools/testing";

// The case's handler.py aborts the build at one command of build.mjs: that
// command and every later one fail without running.
test({ errors: ["aborted the build at command 2"] }, async (t) => {
    await t.cli(["--handler", "python handler.py", "--", "node", "build.mjs"]);
});
