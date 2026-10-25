import { test } from "@catter/tools/testing";

// A directory is where the database goes: --replace refuses it, as a merge
// does, and leaves what it holds.
test({ errors: ["CDB path is not a file"] }, async (t) => {
    await t.cli("compile-commands --replace -o out.json --from events.json");
    t.show("out.json/", t.disk.list("out.json/**"));
});
