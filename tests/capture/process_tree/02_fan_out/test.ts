import { test } from "@catter/tools/testing";

// The case's script.js counts what catter saw of build.mjs's programs.
test(async (t) => {
    await t.run("node build.mjs");
    await t.cli("script.js -- node build.mjs");
});
