import { test } from "@catter/tools/testing";

// Without a generator, CMake on Windows picks the newest Visual Studio: the
// build runs through MSBuild and its CL task. MSBuild builds with one node
// (-m:1), so its output keeps one order, and with the x64 tools for x64 on
// every Windows: on ARM64 it would build for ARM64, with other flags.
const options = {
    except: {
        linux: "MSBuild builds for Windows",
        darwin: "MSBuild builds for Windows",
    },
};

test("hook", options, async (t) => {
    await t.setup("cmake -S . -B build -A x64 -T host=x64");
    await t.cli(
        "compile-commands -o compile_commands.json -- cmake --build build --config Debug -- -m:1 -nologo",
    );
    t.show("compile_commands.json", t.disk.cdb());
});

// MSBuild runs the compiler from the shims, which CLToolPath names, though CMake configured the
// build before catter ran.
test("wrapper", options, async (t) => {
    await t.setup("cmake -S . -B build -A x64 -T host=x64");
    await t.cli(
        "compile-commands --capture wrapper -o compile_commands.json -- cmake --build build --config Debug -- -m:1 -nologo",
    );
    t.show("compile_commands.json", t.disk.cdb());
});
