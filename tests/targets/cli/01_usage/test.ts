import { test } from "@catter/tools/testing";

// The help of targets, and a depth it does not take: neither runs the build.
test({ errors: ["targets: expected --depth >= 0, got -1"] }, async (t) => {
    await t.cli("targets --help -- node -e 0");
    await t.cli("targets -d -1 -- node -e 0");
});
