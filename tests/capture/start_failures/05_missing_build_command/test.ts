import { test } from "@catter/tools/testing";

// The build command names a program that does not exist: it does not start,
// catter tells why, and exits non-zero.
test({ errors: ["spawn failed"] }, async (t) => {
    await t.cli("tree -w 1000 -- catter-test-no-such-program");
});
