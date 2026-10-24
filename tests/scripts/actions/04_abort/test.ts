import { test } from "@catter/tools/testing";

// The case's script.js aborts the build at one command of build.mjs: that
// command and every later one fail without running.
test({ errors: ["The script aborted the build at command 2"] }, async (t) => {
    await t.cli("script.js -- node build.mjs");
});
