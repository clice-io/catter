import { test } from "@catter/tools/testing";

test(async (t) => {
    await t.run("node build.mjs");
    await t.cli("cmd-tree -w 20 -- node build.mjs");
});
