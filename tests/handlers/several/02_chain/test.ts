import { test } from "@catter/tools/testing";

// Two handlers that decide, in the order of the command line: the second
// decides on what the first left, and never sees what it dropped.
test(async (t) => {
    await t.cli([
        "--handler",
        "python decider.py",
        "--handler",
        "python second.py",
        "--",
        "node",
        "build.mjs",
    ]);
});
