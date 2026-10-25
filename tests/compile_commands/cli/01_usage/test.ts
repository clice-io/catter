import { test } from "@catter/tools/testing";

// The help of compile-commands, and an argument it does not take: neither runs the build nor
// writes a database.
test({ errors: ["compile-commands: "] }, async (t) => {
    await t.cli("compile-commands --help -- node -e 0");
    await t.cli("compile-commands a.json -- node -e 0");
    t.show("compile_commands.json", t.disk.cdb("build/compile_commands.json"));
    t.show("a.json", t.disk.cdb("a.json"));
});
