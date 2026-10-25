import { generate, test } from "@catter/tools/testing";

// make -j8 over 100 sources: every source gets an entry. The compiles run
// side by side, so make runs silent (-s): the order of their lines is not
// pinned.
test({ except: { win32: "the Makefile's recipes need a POSIX shell" } }, async (t) => {
    t.disk.write(generate.sources(100));
    await t.cli("compile-commands -o compile_commands.json -- make -j8 -s");
    t.show("compile_commands.json", t.disk.cdb());
});
