import { test } from "@catter/tools/testing";

// A script that does not parse fails with the syntax error before the build
// starts.
test({ errors: ["SyntaxError"] }, async (t) => {
    await t.cli("script.js -- node -e 0");
});
