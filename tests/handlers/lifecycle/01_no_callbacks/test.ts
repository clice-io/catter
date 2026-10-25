import { test } from "@catter/tools/testing";

// A handler that overrides no callback still lets the build run.
test(async (t) => {
    await t.cli([
        "run",
        "--handler",
        "python handler.py",
        "--",
        "node",
        "-e",
        "console.log('the-build-ran')",
    ]);
});
