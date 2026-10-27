import { test } from "@catter/tools/testing";

test("hook", { except: { win32: "the Makefile's recipes need a POSIX shell" } }, async (t) => {
    await t.cli("compile-commands -o compile_commands.json -- make");
    t.show("compile_commands.json", t.disk.cdb());
});

test("wrapper", { except: { win32: "the Makefile's recipes need a POSIX shell" } }, async (t) => {
    await t.cli("compile-commands --capture wrapper -o compile_commands.json -- make");
    t.show("compile_commands.json", t.disk.cdb());
});
