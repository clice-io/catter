import { test } from "@catter/tools/testing";

// The case's script.js aborts one command of build.mjs.
test({ errors: ["IPC request failed: request handler threw"] }, async (t) => {
    await t.cli("script.js -- node build.mjs");
});
