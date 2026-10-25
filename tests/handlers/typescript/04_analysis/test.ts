import { test } from "@catter/tools/testing";

// A handler on the TypeScript SDK gets what catter makes of the commands its
// filter names, edits them, and asks what catter makes of one of its own.
test(async (t) => {
    await t.cli([
        "run",
        "--handler",
        "node handler.ts",
        "--handler",
        "python observer.py",
        "--from",
        "events.json",
    ]);
});
