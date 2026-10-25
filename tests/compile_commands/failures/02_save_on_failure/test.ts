import { test } from "@catter/tools/testing";

// The build fails, and --save-on-failure saves what it compiled.
test(async (t) => {
    await t.cli("compile-commands --save-on-failure -o compile_commands.json --from events.json");
    t.show("compile_commands.json", t.disk.cdb());
});
