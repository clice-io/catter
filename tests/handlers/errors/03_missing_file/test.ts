import { test } from "@catter/tools/testing";

// A handler whose program fails to start its script stops before it answers
// initialize: the build does not start.
test({ errors: ["stopped before it answered initialize"] }, async (t) => {
    await t.cli(["--handler", "python missing.py", "--", "node", "-e", "0"]);
});
