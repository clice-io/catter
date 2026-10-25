import { test } from "@catter/tools/testing";

// A batch file started directly: CreateProcess runs it through cmd.exe,
// which gets its arguments.
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
