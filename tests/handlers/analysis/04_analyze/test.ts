import { test } from "@catter/tools/testing";

// A handler asks catter what it makes of commands of its own, as of those
// catter sends: one analysis for each, none for a program catter knows
// nothing of.
test("at the start", async (t) => {
    await t.cli(["--handler", "python handler.py", "--", "node", "-e", "0"]);
});

// The SDK keeps the messages catter sends while it waits for its answer.
test("while commands come", async (t) => {
    await t.cli(["-m", "replay", "--handler", "python observer.py", "--", "events.json"]);
});

// A request catter cannot read, or does not know, is answered with an error.
test("requests catter cannot answer", async (t) => {
    await t.cli(["--handler", "python raw.py", "--", "node", "-e", "0"]);
});
