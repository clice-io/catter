import { test } from "@catter/tools/testing";

// A run with neither a script nor a handler is reported before the build
// starts.
test({ errors: ["catter needs a script, or a handler"] }, async (t) => {
    await t.cli(["--", "node", "-e", "0"]);
});
