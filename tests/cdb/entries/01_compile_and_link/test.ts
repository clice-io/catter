import { test } from "@catter/tools/testing";

// A compile and a link under make: the compile is an entry, the link and
// make are not.
test(async (t) => {
    await t.cli("-m replay script::cdb -o compile_commands.json -- events.json");
    t.show("compile_commands.json", t.disk.cdb());
});
