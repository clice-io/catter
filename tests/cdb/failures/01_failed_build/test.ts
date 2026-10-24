import { test } from "@catter/tools/testing";

// The build fails: nothing is saved by default.
test(async (t) => {
    await t.cli("-m replay script::cdb -o compile_commands.json -- events.json");
    t.show("compile_commands.json", t.disk.cdb());
});
