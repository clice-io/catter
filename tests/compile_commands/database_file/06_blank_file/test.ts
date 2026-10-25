import { test } from "@catter/tools/testing";

// A database that holds only blanks holds no entries.
test(async (t) => {
    t.disk.write("compile_commands.json", " \n\t\r\n");
    await t.cli("compile-commands -o compile_commands.json --from events.json");
    t.show("compile_commands.json", t.disk.cdb());
});
