import { generate, test } from "@catter/tools/testing";

// Ninja runs one compile per core at once over 100 sources: every one gets
// an entry. The order they finish in is not pinned: Ninja runs quiet.
test(async (t) => {
    t.disk.write(generate.sources(100));
    await t.setup("cmake -S . -B build -G Ninja");
    await t.cli("script::cdb -q -o compile_commands.json -- cmake --build build -- --quiet");
    t.show("compile_commands.json", t.disk.cdb());
});
