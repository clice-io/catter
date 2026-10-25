import { test } from "@catter/tools/testing";

// An unknown built-in is reported, with the names of the built-ins, before
// the build starts.
test({ errors: ["Unknown built-in"] }, async (t) => {
    await t.cli("no-such-builtin -- node -e 0");
});
