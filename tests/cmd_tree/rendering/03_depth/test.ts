import { test } from "@catter/tools/testing";

// --depth shows that many levels of children under the root; 0 shows the root alone.
test(async (t) => {
    await t.cli("-m replay script::cmd-tree -d 1 -- events.json");
    await t.cli("-m replay script::cmd-tree --depth=0 -- events.json");
});
