import { test } from "@catter/tools/testing";

// A program hands its child an inheritable pipe by its handle value: the
// child, started through catter, still has it (the tree leaves out the
// handle's value, which differs from run to run).
test(
    {
        except: {
            linux: "Windows handles and the way Windows starts a program",
            darwin: "Windows handles and the way Windows starts a program",
        },
    },
    async (t) => {
        await t.setup("cl /nologo handle.c");
        await t.run("handle.exe");
        await t.cli("tree -a 0 -- handle.exe");
    },
);
