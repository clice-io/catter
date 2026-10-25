import { test } from "@catter/tools/testing";

// Each case replays the build in its events.json.
test(async (t) => {
    await t.cli("targets --from events.json");
});
