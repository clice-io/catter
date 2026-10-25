import { test } from "@catter/tools/testing";

// cmd.exe parses its own command line: it must receive the one its caller
// wrote, not one rebuilt from split arguments.
test(
    {
        except: {
            linux: "a Windows program and the way Windows starts it",
            darwin: "a Windows program and the way Windows starts it",
        },
    },
    async (t) => {
        await t.setup("cl /nologo spawn.c");
        await t.run("spawn.exe CreateProcessW command.txt");
        await t.cli("tree -w 1000 -- spawn.exe CreateProcessW command.txt");
    },
);
