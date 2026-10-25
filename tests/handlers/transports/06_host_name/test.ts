import { test } from "@catter/tools/testing";

// catter resolves no host names: --connect takes an IP address.
test("host name", { errors: ["names no IP address"] }, async (t) => {
    await t.cli(["run", "--connect", "example.com:9000", "--", "node", "-e", "0"]);
});

test("no host", { errors: ["names no IP address"] }, async (t) => {
    await t.cli(["run", "--connect", ":9000", "--", "node", "-e", "0"]);
});
