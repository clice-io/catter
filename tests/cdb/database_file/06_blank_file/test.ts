import { test } from "@catter/tools/testing";

// A database that holds only blanks holds no entries.
test(async (t) => {
    t.disk.write("compile_commands.json", " \n\t\r\n");
    await t.cli("-m replay cdb -o compile_commands.json -- events.json");
    t.show("compile_commands.json", t.disk.cdb());
});
