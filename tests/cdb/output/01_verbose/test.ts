import { test } from "@catter/tools/testing";

// --verbose with nothing compiled: catter says so instead of staying silent.
test(async (t) => {
    await t.cli("script::cdb --verbose -o compile_commands.json -- node -e 0");
    t.show("compile_commands.json", t.disk.cdb());
});
