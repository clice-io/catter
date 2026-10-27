import { test } from "@catter/tools/testing";

// A program that exists but cannot start, and one started in a directory that
// does not: CreateProcess fails as it does without catter, with the same
// error, and catter tells that the command did not start.
test(
    {
        except: {
            linux: "CreateProcess and its errors are Windows'",
            darwin: "CreateProcess and its errors are Windows'",
        },
    },
    async (t) => {
        await t.setup("cl /nologo start.c");
        await t.run("start.exe not-a-program.exe");
        await t.cli("tree -w 1000 -- start.exe not-a-program.exe");
        await t.run("start.exe start.exe no-such-directory");
        await t.cli("tree -w 1000 -- start.exe start.exe no-such-directory");
    },
);
