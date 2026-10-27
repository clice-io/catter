import { test } from "@catter/tools/testing";

// Ninja runs the compiles one at a time (-j 1), so its output keeps one
// order.
test("hook", async (t) => {
    await t.setup("cmake -S . -B build -G Ninja");
    await t.cli("compile-commands -o compile_commands.json -- cmake --build build -j 1");
    t.show("compile_commands.json", t.disk.cdb());
});

// CMake configured under the wrapper backend runs the compilers through the session's shims,
// which go with the session: the build is configured and built in one.
test("wrapper", { except: { win32: "the wrapper backend runs on Linux and macOS" } }, async (t) => {
    await t.cli([
        "compile-commands",
        "--capture",
        "wrapper",
        "-o",
        "compile_commands.json",
        "--",
        "sh",
        "-c",
        "cmake -S . -B build -G Ninja > /dev/null && cmake --build build -j 1",
    ]);
    t.show("compile_commands.json", t.disk.cdb());
});
