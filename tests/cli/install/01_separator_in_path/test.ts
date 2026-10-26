import { test } from "@catter/tools/testing";

// catter installed in a directory whose path has a space, or a colon: the
// variable that preloads its hook into the build (LD_PRELOAD, or
// DYLD_INSERT_LIBRARIES on macOS) splits paths there, and no escape keeps
// them whole.
test("space", { errors: ["cannot preload its hook"] }, async (t) => {
    await t.run(["node", "run.mjs", "my tools"]);
});

test(
    "colon",
    {
        errors: ["cannot preload its hook"],
        except: { win32: "a Windows path has no colon but the drive's" },
    },
    async (t) => {
        await t.run(["node", "run.mjs", "my:tools"]);
    },
);
