import { test } from "@catter/tools/testing";

// The messages a handler is sent, as one that speaks the protocol itself
// sees them: a handler that decides gets each command as a request; one that
// observes gets notifications, only of the events it asked for.
test("decides", async (t) => {
    await t.cli(["run", "--handler", "python handler.py decides", "--from", "events.json"]);
});

test("observes", async (t) => {
    await t.cli(["run", "--handler", "python handler.py observes", "--from", "events.json"]);
});
