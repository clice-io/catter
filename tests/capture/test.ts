import { test } from "@catter/tools/testing";

// A case's build.mjs starts programs the way a build tool does and prints
// what they received: run directly, then under catter, which shows what it
// captured.
test(async (t) => {
    await t.run("node build.mjs");
    await t.cli("cmd-tree -w 1000 -- node build.mjs");
});
