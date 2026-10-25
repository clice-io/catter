import { test } from "@catter/tools/testing";

// A handler that asks for the environment gets each command's, and changes
// a command's working directory and environment.
test(async (t) => {
    await t.cli(["--handler", "python handler.py", "--", "node", "build.mjs"]);
});
