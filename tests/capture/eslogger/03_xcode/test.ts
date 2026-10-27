import { test } from "@catter/tools/testing";

// xcodebuild runs the compiles from its build service, which it starts, under
// the hardened runtime that keeps the hook out; eslogger sees them.
test(
    {
        except: { linux: "eslogger is macOS's", win32: "eslogger is macOS's" },
        needs: ["eslogger", "xcode"],
    },
    async (t) => {
        await t.setup("cmake -S . -B build -G Xcode");
        await t.cli(
            "compile-commands --capture eslogger -q -o compile_commands.json -- cmake --build build -- -quiet",
        );
        t.show("compile_commands.json", t.disk.cdb());
    },
);
