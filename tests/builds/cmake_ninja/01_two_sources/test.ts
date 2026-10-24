import { test } from "@catter/tools/testing";

// Ninja runs the compiles one at a time (-j 1), so its output keeps one
// order.
test(async (t) => {
    await t.setup("cmake -S . -B build -G Ninja");
    await t.cli("script::cdb -o compile_commands.json -- cmake --build build -j 1");
    t.show("compile_commands.json", t.disk.cdb());
});
