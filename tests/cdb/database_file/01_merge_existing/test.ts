import { test } from "@catter/tools/testing";

// The database exists: its entries stay beside the new ones, the default.
test(async (t) => {
    await t.cli("-m replay cdb -o compile_commands.json -- events.json");
    t.show("compile_commands.json", t.disk.cdb());
});
