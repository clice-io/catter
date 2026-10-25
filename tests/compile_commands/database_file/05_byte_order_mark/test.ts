import { test } from "@catter/tools/testing";

// A database that begins with a byte order mark, as editors on Windows may
// write it, keeps its entries.
test(async (t) => {
    await t.cli("compile-commands -o compile_commands.json --from events.json");
    t.show("compile_commands.json", t.disk.cdb());
});
