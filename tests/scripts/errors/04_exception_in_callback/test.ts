import { test } from "@catter/tools/testing";

// An exception in a script's onCommand ends the session with an error that
// names it.
test({ errors: ["IPC request failed: request handler threw"] }, async (t) => {
    await t.cli(
        "script.js -- node -e \"require('node:child_process').spawnSync('node', ['-e', '0'])\"",
    );
});
