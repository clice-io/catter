import { test } from "@catter/tools/testing";

// catter resolves no host names: --connect takes an IP address.
test({ errors: ["names no IP address"] }, async (t) => {
    await t.cli(["--connect", "example.com:9000", "--", "node", "-e", "0"]);
});
