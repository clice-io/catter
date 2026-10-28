import { test } from "@catter/tools/testing";

// The trace cannot go where a directory is: catter says so once the build has ended, and exits 1.
test({ errors: ["Failed to write the trace"] }, async (t) => {
    await t.cli("profile -o taken --from events.json");
});
