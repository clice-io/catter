import { test } from "@catter/tools/testing";

// Programs of two architectures start each other: spawn.exe is x64, and
// System32's cmd.exe is of the machine's own, arm64 on Windows on ARM. Each
// gets the hook of its architecture.
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
        await t.cli("cmd-tree -w 1000 -- spawn.exe CreateProcessW command.txt");
    },
);
