import { test } from "@catter/tools/testing";

// An observer and a handler that decides, side by side: the observer, first,
// is told of every command as the build started it.
test(async (t) => {
    await t.cli([
        "--handler",
        "python observer.py",
        "--handler",
        "python decider.py",
        "--",
        "node",
        "build.mjs",
    ]);
});
