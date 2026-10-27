import { test } from "@catter/tools/testing";

// xmake's build cache is off here (02_build_cache has it on), so each source
// is compiled in one step. xmake also compiles files of its own in the
// temporary directory to probe the compiler's flags: they get no entries.
test(
    {
        except: {
            win32: "xmake 3.0.8 does not find a CI runner's Visual Studio 2026 from a cold start: the build fails without catter too",
        },
    },
    async (t) => {
        await t.setup("xmake config -P . --ccache=n --yes");
        await t.cli("compile-commands -q -o compile_commands.json -- xmake build -P . -j 1");
        t.show("compile_commands.json", t.disk.cdb());
    },
);
