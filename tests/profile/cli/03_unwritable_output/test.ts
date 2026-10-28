import { test } from "@catter/tools/testing";

// The trace cannot go where a directory is, nor in a directory where a file is: catter says so
// once the build has ended, and exits 1.
test({ errors: ["Failed to write the trace"] }, async (t) => {
    await t.cli("profile -o taken --from events.json");
    await t.cli("profile -o taken/kept.txt/trace.json --from events.json");
});
