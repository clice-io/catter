import { test } from "@catter/tools/testing";

// A .NET Framework program built for any CPU: a 32-bit executable Windows runs
// as a 64-bit process, x64 on Windows on ARM. Detours makes its header a
// 64-bit one when it adds the hook.
test(
    {
        except: {
            linux: "a Windows program and the way Windows starts it",
            darwin: "a Windows program and the way Windows starts it",
        },
    },
    async (t) => {
        await t.setup("cl /nologo spawn.c");
        await t.setup(
            "C:/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe /nologo /platform:anycpu app.cs",
        );
        await t.run("spawn.exe CreateProcessW command.txt");
        await t.cli("cmd-tree -w 1000 -- spawn.exe CreateProcessW command.txt");
    },
);
