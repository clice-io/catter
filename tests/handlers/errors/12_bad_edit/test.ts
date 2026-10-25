import { test } from "@catter/tools/testing";

// An edit catter cannot make ends the session with an error that names it.
test("an input", { errors: ["names no option to remove"] }, async (t) => {
    await t.cli(["run", "--handler", "python handler.py input", "--from", "events.json"]);
});

test("a program catter reads no arguments of", { errors: ["reads no arguments"] }, async (t) => {
    await t.cli(["run", "--handler", "python handler.py program", "--from", "events.json"]);
});
