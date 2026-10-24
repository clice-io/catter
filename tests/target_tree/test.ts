import { test } from "@catter/tools/testing";

// Each case replays the build in its events.json.
test(async (t) => {
    await t.cli("-m replay script::target-tree -- events.json");
});
