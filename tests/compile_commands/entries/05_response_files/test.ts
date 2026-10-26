import { test } from "@catter/tools/testing";

// The compiler takes its arguments from a response file that is gone once it
// has run, as MSBuild's are: the entries hold the arguments the file held.
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
