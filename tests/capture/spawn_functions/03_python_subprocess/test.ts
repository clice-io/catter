import { test } from "@catter/tools/testing";

test(async (t) => {
    await t.run("python build.py");
    await t.cli("tree -w 1000 -- python build.py");
});
