import { test } from "@catter/tools/testing";

// The help of cmd-tree and a width it does not take, neither of which runs the build,
// and a width given after `=`.
test({ errors: ["cmd-tree: expected --arg-width >= 4, got 2"] }, async (t) => {
    await t.cli("cmd-tree --help -- node -e 0");
    await t.cli("cmd-tree -w 2 -- node -e 0");
    await t.cli("cmd-tree --arg-width=4 -- node -e 0");
});
