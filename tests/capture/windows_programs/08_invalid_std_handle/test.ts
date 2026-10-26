import { test } from "@catter/tools/testing";

// A program gives its child a standard handle the child does not have: the
// child, started through catter, runs without that stream, as it would
// without catter.
test(
    {
        except: {
            linux: "Windows handles and the way Windows starts a program",
            darwin: "Windows handles and the way Windows starts a program",
        },
    },
    async (t) => {
        await t.setup("cl /nologo invalid.c");
        await t.run("invalid.exe");
        await t.cli("tree -a 0 -- invalid.exe");
    },
);
