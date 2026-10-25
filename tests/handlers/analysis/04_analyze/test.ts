import { test } from "@catter/tools/testing";

// A handler asks catter what it makes of commands of its own, as of those
// catter sends: one analysis for each, none for a program catter knows
// nothing of.
test(async (t) => {
    await t.cli(["--handler", "python handler.py", "--", "node", "-e", "0"]);
});
