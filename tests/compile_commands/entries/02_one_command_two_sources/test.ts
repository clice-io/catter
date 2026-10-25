import { test } from "@catter/tools/testing";

// One command compiles and links two sources, as small Makefiles and
// scripts do: each source gets an entry.
test("posix", { except: { win32: "the POSIX compiler driver" } }, async (t) => {
    await t.cli(
        "compile-commands -o compile_commands.json -- c++ -DCATTER_TEST=1 -Iinclude src/main.cc src/util.cc -o hello",
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
            "compile-commands -o compile_commands.json -- cl /nologo /EHsc /DCATTER_TEST=1 /Iinclude src/main.cc src/util.cc /Fehello.exe",
        );
        t.show("compile_commands.json", t.disk.cdb());
    },
);
