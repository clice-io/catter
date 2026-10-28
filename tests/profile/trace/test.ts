import { test } from "@catter/tools/testing";

// Each case replays the build in its events.json, which records when each command started and
// how long it ran, and shows the trace whole.
test(async (t) => {
    await t.cli("profile --from events.json");
    t.show("trace.json", t.disk.json("trace.json"));
});
