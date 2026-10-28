import { test } from "@catter/tools/testing";

// The compile in the directory the build removes gets the entry of the
// directory its symlinks lead to; the one in the directory it keeps stays
// there.
test("posix", { except: { win32: "the POSIX compiler driver" } }, async (t) => {
    await t.cli("compile-commands -o compile_commands.json -- node build.mjs posix");
    t.show("compile_commands.json", t.disk.cdb());
});

test(
    "msvc",
    {
        except: {
            linux: "cl.exe builds for Windows",
            darwin: "cl.exe builds for Windows",
        },
    },
    async (t) => {
        await t.cli("compile-commands -o compile_commands.json -- node build.mjs msvc");
        t.show("compile_commands.json", t.disk.cdb());
    },
);
