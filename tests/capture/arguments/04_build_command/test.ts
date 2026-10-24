import { test } from "@catter/tools/testing";

// The build command catter itself starts gets its arguments as given:
// spaces, quotes, empty and non-ASCII ones.
test(async (t) => {
    const command = ["node", "child.mjs", "with space", 'quote"inside', "", "中文-é"];
    await t.run(command);
    await t.cli(["script::cmd-tree", "-w", "1000", "--", ...command]);
});
