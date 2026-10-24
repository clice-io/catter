import { test } from "@catter/tools/testing";

// The case's script.js prints what each callback receives: under inject,
// under replay, and when its onStart runs no build.
test("inject", async (t) => {
    await t.cli("script.js append -- node build.mjs");
});

test("replay", async (t) => {
    await t.cli("-m replay script.js -- events.json");
});

test("no build", async (t) => {
    await t.cli("script.js stop -- node build.mjs");
});
