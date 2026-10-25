import { test } from "@catter/tools/testing";

// A build under catter leaves Ninja's dependency records as a direct build
// does: building again right after has nothing to do.
test(async (t) => {
    await t.setup("cmake -S . -B build -G Ninja");
    await t.cli("compile-commands -q -o compile_commands.json -- cmake --build build -j 1");
    await t.run("cmake --build build -j 1");
});
