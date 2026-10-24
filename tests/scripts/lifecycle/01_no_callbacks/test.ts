import { test } from "@catter/tools/testing";

// A script that registers no callbacks still lets the build run.
test({ errors: ["service.onStart is not registered"] }, async (t) => {
    await t.cli("script.js -- node -e \"console.log('the build ran')\"");
});
