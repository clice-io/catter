import { test } from "@catter/tools/testing";

// The case's handler.py counts what catter saw of build.mjs's programs.
test(async (t) => {
    await t.run("node build.mjs");
    await t.cli(["run", "--handler", "python handler.py", "--", "node", "build.mjs"]);
});
