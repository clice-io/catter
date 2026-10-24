import { test } from "@catter/tools/testing";

// An observer on the TypeScript SDK whose callbacks wait: each runs to its
// end before the next message's.
test(async (t) => {
    await t.cli(["--handler", "node handler.ts", "--", "node", "build.mjs"]);
});
