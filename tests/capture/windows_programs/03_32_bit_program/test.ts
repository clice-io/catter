import { test } from "@catter/tools/testing";

// A 64-bit program starts a 32-bit one, SysWOW64's cmd.exe.
test(
    {
        except: {
            linux: "a Windows program and the way Windows starts it",
            darwin: "a Windows program and the way Windows starts it",
        },
        // The hook, a 64-bit DLL, cannot be injected into a 32-bit program
        // (catter#124).
        errors: [
            "Injection with method \\w+ failed",
            "Failed to inject DLL into target process",
            "Process is still active during cleanup",
        ],
    },
    async (t) => {
        await t.setup("cl /nologo spawn.c");
        await t.run("spawn.exe CreateProcessW command.txt");
        await t.cli("cmd-tree -w 1000 -- spawn.exe CreateProcessW command.txt");
    },
);
