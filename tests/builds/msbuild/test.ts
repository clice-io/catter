import { test } from "@catter/tools/testing";

// Without a generator, CMake on Windows picks the newest Visual Studio: the
// build runs through MSBuild and its CL task. MSBuild builds with one node
// (-m:1), so its output keeps one order.
test(
    {
        except: {
            linux: "MSBuild builds for Windows",
            darwin: "MSBuild builds for Windows",
        },
    },
    async (t) => {
        await t.setup("cmake -S . -B build");
        await t.cli(
            "script::cdb -o compile_commands.json -- cmake --build build --config Debug -- -m:1 -nologo",
        );
        t.show("compile_commands.json", t.disk.cdb());
    },
);
