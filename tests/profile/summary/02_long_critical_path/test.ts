import { test } from "@catter/tools/testing";

// A critical path of eight steps shows its first three and its last two.
test(async (t) => {
    await t.cli("profile --from events.json");
});
