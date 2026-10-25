import { test } from "@catter/tools/testing";

// An observer on the TypeScript SDK whose callbacks wait: each runs to its
// end before the next message's, even when the messages come one right after
// another, as a replayed build sends them.
test("inject", async (t) => {
    await t.cli(["run", "--handler", "node handler.ts", "--", "node", "build.mjs"]);
});

test("replay", async (t) => {
    await t.cli(["run", "--handler", "node handler.ts", "--from", "events.json"]);
});
