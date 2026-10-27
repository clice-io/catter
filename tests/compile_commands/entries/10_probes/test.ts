import { test } from "@catter/tools/testing";

// A probe compiles a file the build removes before it ends: only the
// project's source gets an entry.
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
