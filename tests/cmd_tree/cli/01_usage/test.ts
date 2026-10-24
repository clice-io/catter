import { test } from "@catter/tools/testing";

// The help of script::cmd-tree and a width it does not take, neither of which runs the build,
// and a width given after `=`.
test({ errors: ["cmd-tree: expected --argWidth >= 4, got 2"] }, async (t) => {
    await t.cli("script::cmd-tree --help -- node -e 0");
    await t.cli("script::cmd-tree -w 2 -- node -e 0");
    await t.cli("script::cmd-tree --argWidth=4 -- node -e 0");
});
