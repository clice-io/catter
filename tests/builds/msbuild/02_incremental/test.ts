import { test } from "@catter/tools/testing";

// MSBuild records which files each compile read and wrote (its tracker
// enters cl.exe) and skips an up-to-date project the next time: after a
// build under catter, building again compiles nothing.
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
            "cdb -q -o compile_commands.json -- cmake --build build --config Debug -- -m:1 -nologo",
        );
        await t.run("cmake --build build --config Debug -- -m:1 -nologo");
    },
);
