import { test } from "@catter/tools/testing";

// Each line of the case's commands.txt goes to explain, which
// analyzes the command without running it: no case holds the files its
// commands compile, only the response files they name.
test(async (t) => {
    const lines = t.disk.read("commands.txt").split("\n");
    for (const line of lines.filter((line) => line.trim() !== "")) {
        await t.cli(`explain -- ${line}`);
    }
});
