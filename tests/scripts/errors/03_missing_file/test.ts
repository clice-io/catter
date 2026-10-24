import { test } from "@catter/tools/testing";

// A script file that does not exist is reported before the build starts.
test({ errors: ["Failed to open script file"] }, async (t) => {
    await t.cli("missing.js -- node -e 0");
});
