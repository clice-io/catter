import { test } from "@catter/tools/testing";

// An exception in a script's onCommand ends the session with an error that
// names it: the command it was deciding does not run.
test({ errors: ["thrown from onCommand"] }, async (t) => {
    await t.cli(
        "script.js -- node -e \"require('node:child_process').spawnSync('node', ['-e', '0'])\"",
    );
});
