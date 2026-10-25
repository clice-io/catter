import { test } from "@catter/tools/testing";

// An exception in a handler's on_command ends the session with an error
// that names it: the command it was deciding does not run.
test({ errors: ["failed on command 1"] }, async (t) => {
    await t.cli([
        "run",
        "--handler",
        "python handler.py",
        "--",
        "node",
        "-e",
        "require('node:child_process').spawnSync('node', ['-e', '0'])",
    ]);
});
