import { test } from "@catter/tools/testing";

// make -j4 runs a sub-make: it still gets the jobserver (make warns when it
// does not), and both of its compiles get entries.
test("hook", { except: { win32: "the Makefile's recipes need a POSIX shell" } }, async (t) => {
    await t.cli("compile-commands -o compile_commands.json -- make -j4");
    t.show("compile_commands.json", t.disk.cdb());
});

test("wrapper", { except: { win32: "the Makefile's recipes need a POSIX shell" } }, async (t) => {
    await t.cli("compile-commands --capture wrapper -o compile_commands.json -- make -j4");
    t.show("compile_commands.json", t.disk.cdb());
});
