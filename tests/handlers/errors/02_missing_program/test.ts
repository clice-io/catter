import { test } from "@catter/tools/testing";

// A handler whose program does not exist is reported before the build
// starts.
test({ errors: ["Failed to start handler"] }, async (t) => {
    await t.cli(["run", "--handler", "no-such-handler", "--", "node", "-e", "0"]);
});
