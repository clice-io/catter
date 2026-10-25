import { test } from "@catter/tools/testing";

// The build fails: nothing is saved by default.
test(async (t) => {
    await t.cli("compile-commands -o compile_commands.json --from events.json");
    t.show("compile_commands.json", t.disk.cdb());
});
