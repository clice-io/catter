import { test } from "@catter/tools/testing";

// The help of profile and a --top it does not take, neither of which runs the build.
test({ errors: ["profile: expected --top >= 0, got -1"] }, async (t) => {
    await t.cli("profile --help -- node -e 0");
    await t.cli("profile --top -1 -- node -e 0");
});
