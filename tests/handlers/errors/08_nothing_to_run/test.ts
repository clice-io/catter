import { test } from "@catter/tools/testing";

// run without a handler is reported before the build starts.
test({ errors: ["run: name a handler"] }, async (t) => {
    await t.cli(["run", "--", "node", "-e", "0"]);
});
