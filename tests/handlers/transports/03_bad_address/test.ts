import { test } from "@catter/tools/testing";

// An address that is not host:port is reported before the build starts.
test({ errors: ["is not an address for --connect"] }, async (t) => {
    await t.cli(["--connect", "localhost", "--", "node", "-e", "0"]);
});
