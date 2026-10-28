import { test } from "@catter/tools/testing";

// -q prints nothing, and writes the trace all the same.
test(async (t) => {
    await t.cli("profile -q --from events.json");
    t.show("trace.json", t.disk.trace());
});
