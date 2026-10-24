import { test } from "@catter/tools/testing";

// With CMAKE_NINJA_FORCE_RESPONSE_FILE, Ninja passes each compile's flags
// and source through a response file (@file), as it does for long command
// lines and MSBuild always does.
test(
    {
        except: {
            win32: "cl.exe reads a response file in the ANSI code page, which cannot spell the copy's root: the build fails without catter too",
        },
    },
    async (t) => {
        await t.setup("cmake -S . -B build -G Ninja -DCMAKE_NINJA_FORCE_RESPONSE_FILE=ON");
        await t.cli("script::cdb -q -o compile_commands.json -- cmake --build build -j 1");
        t.show("compile_commands.json", t.disk.cdb());
    },
);
