import { test } from "@catter/tools/testing";

// An unknown builtin script is reported before the build starts.
test({ errors: ["Unknown builtin script"] }, async (t) => {
    await t.cli("script::no-such-script -- node -e 0");
});
