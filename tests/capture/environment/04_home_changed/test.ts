import { test } from "@catter/tools/testing";

test(async (t) => {
    await t.cli("tree -w 1000 -- node build.mjs");
    t.show("home/", t.disk.list("home/**"));
});
