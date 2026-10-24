import { test } from "@catter/tools/testing";

// A handler on the TypeScript SDK decides on the commands of build.mjs and
// is told how they ended.
test(async (t) => {
    await t.cli(["--handler", "node handler.ts", "--", "node", "build.mjs"]);
});
