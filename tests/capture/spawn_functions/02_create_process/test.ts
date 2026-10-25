import { test } from "@catter/tools/testing";

// Both spellings of CreateProcess start a program catter sees, with an
// argument past ASCII that the ANSI code page holds (é).
test(
    {
        except: {
            linux: "CreateProcess is the Windows API",
            darwin: "CreateProcess is the Windows API",
        },
    },
    async (t) => {
        await t.setup("cl /nologo spawn.c");
        for (const name of ["CreateProcessA", "CreateProcessW"]) {
            await t.cli(["cmd-tree", "-w", "1000", "--", "spawn.exe", name, "command.txt"]);
        }
    },
);
