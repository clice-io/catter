import { test } from "@catter/tools/testing";

// The two commands that took the longest, then none.
test(async (t) => {
    await t.cli("profile --top 2 --from events.json");
    await t.cli("profile --top=0 --from events.json");
});
