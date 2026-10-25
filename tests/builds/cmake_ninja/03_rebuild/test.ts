import { test } from "@catter/tools/testing";

// Building again into the same database, which the default merges with,
// keeps one entry per compile instead of adding duplicates.
test(async (t) => {
    await t.setup("cmake -S . -B build -G Ninja");
    const build = "compile-commands -q -o compile_commands.json -- cmake --build build -j 1";
    await t.cli(build);
    await t.setup("cmake --build build --target clean");
    await t.cli(build);
    t.show("compile_commands.json", t.disk.cdb());
});
