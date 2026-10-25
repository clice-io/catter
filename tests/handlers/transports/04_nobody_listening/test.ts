import { test } from "@catter/tools/testing";

// A port no handler listens on is reported before the build starts.
test({ errors: ["Failed to connect to handler"] }, async (t) => {
    await t.cli(["run", "--connect", "127.0.0.1:1", "--", "node", "-e", "0"]);
});
