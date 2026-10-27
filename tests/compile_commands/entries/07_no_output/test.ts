import { test } from "@catter/tools/testing";

// Commands that write no object from their sources: a check of the syntax
// (-fsyntax-only, /Zs), a compile and link to the default a.out, a preprocess
// to stdout and the dependencies of a source (-MM). Each source gets an entry,
// with the output a command writes from it, if any.
test(async (t) => {
    await t.cli("compile-commands -o compile_commands.json --from events.json");
    t.show("compile_commands.json", t.disk.cdb());
});
