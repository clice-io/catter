import { test } from "@catter/tools/testing";

// The build command catter itself starts gets its arguments as given:
// spaces, quotes, empty and non-ASCII ones.
test("hook", async (t) => {
    const command = ["node", "child.mjs", "with space", 'quote"inside', "", "中文-é"];
    await t.run(command);
    await t.cli(["tree", "-w", "1000", "--", ...command]);
});

const linuxOnly = {
    except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" },
};

test("ptrace", linuxOnly, async (t) => {
    const command = ["node", "child.mjs", "with space", 'quote"inside', "", "中文-é"];
    await t.cli(["tree", "--capture", "ptrace", "-w", "1000", "--", ...command]);
});
