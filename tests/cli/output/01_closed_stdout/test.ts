import { test } from "@catter/tools/testing";

test(async (t) => {
    await t.run("node stdout.mjs");
});
