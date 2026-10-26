import { test } from "@catter/tools/testing";

// The database has a source compiled to two objects, a static and a shared
// one; the build compiles it to one of them again: its entry is replaced, and
// the other one stays.
test(async (t) => {
    await t.cli("compile-commands -o compile_commands.json --from events.json");
    t.show("compile_commands.json", t.disk.cdb());
});
