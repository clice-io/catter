import { test } from "@catter/tools/testing";

// A C++20 modules build: CMake scans each source for the modules it
// imports, then compiles it with a module map. Each source gets an entry
// carrying its flags.
test({ except: { darwin: "Apple's clang comes without clang-scan-deps" } }, async (t) => {
    await t.setup("cmake -S . -B build -G Ninja");
    await t.cli("script::cdb -q -o compile_commands.json -- cmake --build build -j 1");
    t.show("compile_commands.json", t.disk.cdb());
});
