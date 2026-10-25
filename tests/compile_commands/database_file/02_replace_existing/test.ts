import { test } from "@catter/tools/testing";

// The database exists: --replace drops its entries.
test(async (t) => {
    await t.cli("compile-commands --replace -o compile_commands.json --from events.json");
    t.show("compile_commands.json", t.disk.cdb());
});
