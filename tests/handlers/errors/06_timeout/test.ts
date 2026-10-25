import { test } from "@catter/tools/testing";

// A handler that does not answer a command in time ends the session: that
// command and every later one fail without running, and catter ends the
// handler.
test({ errors: ["did not answer command 2 within 5 s"] }, async (t) => {
    await t.cli([
        "run",
        "--handler",
        "python handler.py",
        "--handler-timeout",
        "5",
        "--",
        "node",
        "build.mjs",
    ]);
});
