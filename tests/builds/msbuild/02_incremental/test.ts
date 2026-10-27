import { test } from "@catter/tools/testing";

// MSBuild records which files each compile and link read and wrote (its
// tracker enters cl.exe and link.exe, which it starts suspended) and skips an
// up-to-date project the next time: after a build under catter, its tracking
// logs are those of a build without it, and building again compiles nothing.
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
            "compile-commands -q -o compile_commands.json -- cmake --build build --config Debug -- -m:1 -nologo",
        );
        t.show("hello.tlog/", t.disk.list("build/hello.dir/Debug/hello.tlog/**"));
        await t.run("cmake --build build --config Debug -- -m:1 -nologo");
    },
);
