import { test } from "@catter/tools/testing";

// A 64-bit program starts a 32-bit one, SysWOW64's cmd.exe, which starts a
// 64-bit one again. catter has no hook for 32-bit programs: cmd.exe runs
// without one, and the commands it starts are not captured.
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
