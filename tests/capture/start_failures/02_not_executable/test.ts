import { test } from "@catter/tools/testing";

test({ except: { win32: "Windows has no execute permission" } }, async (t) => {
    await t.run("node build.mjs");
    await t.cli("script::cmd-tree -w 1000 -- node build.mjs");
});
