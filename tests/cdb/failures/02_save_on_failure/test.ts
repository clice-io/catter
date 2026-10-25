import { test } from "@catter/tools/testing";

// The build fails, and --save-on-failure saves what it compiled.
test(async (t) => {
    await t.cli("-m replay cdb --save-on-failure -o compile_commands.json -- events.json");
    t.show("compile_commands.json", t.disk.cdb());
});
