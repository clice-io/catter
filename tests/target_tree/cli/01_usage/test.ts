import { test } from "@catter/tools/testing";

// The help of script::target-tree, and a depth it does not take: neither runs the build.
test({ errors: ["target-tree: expected --depth >= 0, got -1"] }, async (t) => {
    await t.cli("script::target-tree --help -- node -e 0");
    await t.cli("script::target-tree -d -1 -- node -e 0");
});
