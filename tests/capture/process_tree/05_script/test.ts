import { test } from "@catter/tools/testing";

test({ except: { win32: "Windows starts no script by its #! line" } }, async (t) => {
    await t.run("node build.mjs");
    await t.cli("tree -w 1000 -- node build.mjs");
});
