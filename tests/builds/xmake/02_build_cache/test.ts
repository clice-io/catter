import { test } from "@catter/tools/testing";

// xmake's build cache, which the project turns on, compiles in two steps:
// it preprocesses each source (-E) and compiles what it wrote. The entries
// are the compiles of the sources, made of the preprocesses' arguments. A
// rebuild finds the objects in the cache and only preprocesses: the entries
// are the same compiles, without the objects.
test(
    {
        except: {
            win32: "xmake 3.0.8 does not find a CI runner's Visual Studio 2026 from a cold start: the build fails without catter too",
        },
    },
    async (t) => {
        await t.setup("xmake config -P . --yes");
        await t.cli("compile-commands -q -o compile_commands.json -- xmake build -P . -j 1");
        t.show("compile_commands.json", t.disk.cdb());
        await t.cli(
            "compile-commands -q --replace -o compile_commands.json -- xmake build -r -P . -j 1",
        );
        t.show("compile_commands.json after a rebuild", t.disk.cdb());
    },
);
