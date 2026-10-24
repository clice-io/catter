import { test } from "@catter/tools/testing";

// xmake's build cache compiles in two steps, preprocessing each source
// first (-E) and then compiling the preprocessed file: it is off here so
// the entries come from the plain compile. xmake also compiles temporary
// files to probe the compiler's flags; those are entries too.
test(
    {
        except: {
            win32: "xmake 3.0.8 does not find a CI runner's Visual Studio 2026 from a cold start: the build fails without catter too",
        },
    },
    async (t) => {
        await t.setup("xmake config -P . --ccache=n --yes");
        await t.cli("script::cdb -q -o compile_commands.json -- xmake build -P . -j 1");
        t.show("compile_commands.json", t.disk.cdb());
    },
);
