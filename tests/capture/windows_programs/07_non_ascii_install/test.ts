import { test } from "@catter/tools/testing";

// catter installed under the copy's root, whose name is not ASCII: a process
// reads the name of the hook DLL in its import table in its own code page,
// so the hook goes there by an ASCII path.
test(
    {
        except: {
            linux: "a Windows program and the way Windows starts it",
            darwin: "a Windows program and the way Windows starts it",
        },
    },
    async (t) => {
        await t.setup("cl /nologo spawn.c");
        await t.setup("node install.mjs");
        await t.run(
            "bin/catter.exe cmd-tree -w 1000 -- spawn.exe CreateProcessW command.txt",
        );
    },
);
