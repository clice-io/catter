import { test } from "@catter/tools/testing";

// run.mjs starts catter itself, on streams that are no pipes of the runner:
// what the build's programs find there tells whether they are catter's own.
test(async (t) => {
    await t.run("node run.mjs");
});
