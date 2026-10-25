import { test } from "@catter/tools/testing";

// --allow-remote lets catter connect to an address of another machine, with
// a warning: 0.0.0.0, which no handler listens on here.
test({ errors: ["Failed to connect to handler"] }, async (t) => {
    await t.cli(["run", "--allow-remote", "--connect", "0.0.0.0:1", "--", "node", "-e", "0"]);
});
