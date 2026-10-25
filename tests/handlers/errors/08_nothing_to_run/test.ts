import { test } from "@catter/tools/testing";

// A run with neither a built-in nor a handler is reported before the build
// starts.
test({ errors: ["catter needs a built-in, or a handler"] }, async (t) => {
    await t.cli(["--", "node", "-e", "0"]);
});
