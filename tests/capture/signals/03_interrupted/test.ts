import { test } from "@catter/tools/testing";

// Ctrl+C on the terminal catter runs on stops the build, as it stops the build
// run without catter; catter, and a handler it started, see it end, and catter
// ends as it did.
test({ except: { win32: "the test types Ctrl+C on a POSIX pseudo-terminal" } }, async (t) => {
    await t.run("python interrupt.py node build.mjs");
    await t.run("python interrupt.py catter tree -w 1000 -- node build.mjs");
    await t.run([
        "python",
        "interrupt.py",
        "catter",
        "run",
        "--handler",
        "python handler.py",
        "--",
        "node",
        "build.mjs",
    ]);
});
