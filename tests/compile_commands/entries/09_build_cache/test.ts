import { test } from "@catter/tools/testing";

// xmake's build cache preprocesses each source and compiles what the
// preprocess wrote: main.cc's entry is the preprocess's command made the
// compile of main.cc into main.cc.o. A header preprocessed to a file is no
// source of the build.
test(async (t) => {
    await t.cli("compile-commands -o compile_commands.json --from events.json");
    t.show("compile_commands.json", t.disk.cdb());
});
