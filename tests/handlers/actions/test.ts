import { test } from "@catter/tools/testing";

// The case's handler.py acts on one command of build.mjs.
test(async (t) => {
    await t.cli(["--handler", "python handler.py", "--", "node", "build.mjs"]);
});
