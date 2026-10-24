import { test } from "@catter/tools/testing";

// A handler that does not parse stops before it answers initialize: the
// build does not start.
test({ errors: ["stopped before it answered initialize"] }, async (t) => {
    await t.cli(["--handler", "python handler.py", "--", "node", "-e", "0"]);
});
