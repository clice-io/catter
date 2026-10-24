import { test } from "@catter/tools/testing";

// An exception in a script's onExecution ends the session: the commands that
// start after it fail without running.
test({ errors: ["command 2 exited with 3"] }, async (t) => {
    await t.cli("script.js -- node build.mjs");
});
