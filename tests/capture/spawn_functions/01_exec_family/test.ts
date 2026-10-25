import { test } from "@catter/tools/testing";

// Every function of the exec family and posix_spawn starts a program
// catter sees; those that search PATH get its name, the others its path.
const functions: [string, string][] = [
    ["execv", process.execPath],
    ["execve", process.execPath],
    ["execvp", "node"],
    ["execl", process.execPath],
    ["execle", process.execPath],
    ["execlp", "node"],
    ["posix_spawn", process.execPath],
    ["posix_spawnp", "node"],
];

test("posix", { except: { win32: "the POSIX functions" } }, async (t) => {
    await t.setup("cc -o spawn spawn.c");
    for (const [name, program] of functions) {
        await t.cli(["cmd-tree", "-w", "1000", "--", "./spawn", name, program, "child.mjs"]);
    }
});

test(
    "execvpe",
    { except: { darwin: "a GNU extension", win32: "the POSIX functions" } },
    async (t) => {
        await t.setup("cc -o spawn spawn.c");
        await t.cli("cmd-tree -w 1000 -- ./spawn execvpe node child.mjs");
    },
);
