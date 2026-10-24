import { test } from "@catter/tools/testing";

// The case's script.js acts on one command of build.mjs.
test(async (t) => {
    await t.cli("script.js -- node build.mjs");
});
