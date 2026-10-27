import { test } from "@catter/tools/testing";

// xmake's build cache preprocesses each source and compiles what the
// preprocess wrote, unless its cache has the object already: main.cc's entry
// is the preprocess's command made the compile of main.cc into main.cc.o;
// util.cc, whose object came from the cache, keeps its preprocess's.
test(async (t) => {
    await t.cli("compile-commands -o compile_commands.json --from events.json");
    t.show("compile_commands.json", t.disk.cdb());
});
