import { test } from "@catter/tools/testing";

// A handler that writes to its stdout what is not a protocol message is
// reported as such: the build does not start.
test({ errors: ["wrote what is not an answer"] }, async (t) => {
    await t.cli(["--handler", "python handler.py", "--", "node", "-e", "0"]);
});
