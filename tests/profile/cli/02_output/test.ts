import { test } from "@catter/tools/testing";

// The trace goes to trace.json in the directory the build runs in, or where -o says, relative to
// that directory, in a directory catter makes when there is none.
test(async (t) => {
    await t.cli("profile -q --from events.json");
    await t.cli("profile -q -C sub --from ../events.json");
    await t.cli("profile -q -o out/build.json --from events.json");
    await t.cli("profile -q --output=other.json --from events.json");
    t.show("files", t.disk.list("**/*.json"));
});
