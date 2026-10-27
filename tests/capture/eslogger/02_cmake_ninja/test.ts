import { test } from "@catter/tools/testing";

// Ninja runs each command through Apple's /bin/sh, which runs without the
// hook; eslogger sees what it starts. Ninja runs the compiles one at a time
// (-j 1), so its output keeps one order.
test(
    {
        except: { linux: "eslogger is macOS's", win32: "eslogger is macOS's" },
        needs: ["eslogger"],
    },
    async (t) => {
        await t.setup("cmake -S . -B build -G Ninja");
        await t.cli(
            "compile-commands --capture eslogger -o compile_commands.json -- cmake --build build -j 1",
        );
        t.show("compile_commands.json", t.disk.cdb());
    },
);
