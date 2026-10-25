import { test } from "@catter/tools/testing";

// A handler timeout of 0 waits as long as the handler takes.
test(async (t) => {
    await t.cli([
        "run",
        "--handler",
        "python handler.py",
        "--handler-timeout",
        "0",
        "--",
        "node",
        "build.mjs",
    ]);
});
