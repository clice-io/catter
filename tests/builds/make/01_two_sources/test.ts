import { test } from "@catter/tools/testing";

test({ except: { win32: "the Makefile's recipes need a POSIX shell" } }, async (t) => {
    await t.cli("script::cdb -o compile_commands.json -- make");
    t.show("compile_commands.json", t.disk.cdb());
});
