import { test } from "@catter/tools/testing";

test({ except: { win32: "Windows has no signals" } }, async (t) => {
    await t.setup("cc -o report report.c");
    await t.run("node build.mjs");
    await t.cli("tree -w 1000 -- node build.mjs");
    await t.cli("tree -w 1000 -- ./report");
    await t.run("node catter.mjs");
});
