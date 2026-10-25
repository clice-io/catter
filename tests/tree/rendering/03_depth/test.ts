import { test } from "@catter/tools/testing";

// --depth shows that many levels of children under the root; 0 shows the root alone.
test(async (t) => {
    await t.cli("tree -d 1 --from events.json");
    await t.cli("tree --depth=0 --from events.json");
});
