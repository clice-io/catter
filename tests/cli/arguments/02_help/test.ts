import { test } from "@catter/tools/testing";

// catter's own help: its command line and options.
test(async (t) => {
    await t.cli("--help");
});
