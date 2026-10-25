import { test } from "@catter/tools/testing";

// -o into a directory that does not exist yet creates it.
test("posix", { except: { win32: "the POSIX compiler driver" } }, async (t) => {
    await t.cli(
        "compile-commands -o new/dir/compile_commands.json -- c++ -c -DCATTER_TEST=1 -Iinclude src/main.cc -o main.o",
    );
    t.show("new/dir/compile_commands.json", t.disk.cdb("new/dir/compile_commands.json"));
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
            "compile-commands -o new/dir/compile_commands.json -- cl /nologo /c /DCATTER_TEST=1 /Iinclude src/main.cc",
        );
        t.show("new/dir/compile_commands.json", t.disk.cdb("new/dir/compile_commands.json"));
    },
);
