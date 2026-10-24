import { test } from "@catter/tools/testing";

// A compile that only checks the syntax, as linters and editors run it,
// still says how the source compiles.
test("posix", { except: { win32: "the POSIX compiler driver" } }, async (t) => {
    await t.cli(
        "script::cdb -o compile_commands.json -- c++ -fsyntax-only -DCATTER_TEST=1 -Iinclude src/main.cc",
    );
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
        await t.cli(
            "script::cdb -o compile_commands.json -- cl /nologo /Zs /DCATTER_TEST=1 /Iinclude src/main.cc",
        );
        t.show("compile_commands.json", t.disk.cdb());
    },
);
