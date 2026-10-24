import { test } from "@catter/tools/testing";

test(async (t) => {
    await t.run("node build.mjs");
    await t.cli("script::cmd-tree -w 20 -- node build.mjs");
});
