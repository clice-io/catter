import { test } from "@catter/tools/testing";

// A handler is told only of the commands its filter names, and of their
// exits: the others run as they are, the build never waiting for it. A
// command whose arguments do not read is named by its program alone.
test("phases and tools", async (t) => {
    await t.cli(["-m", "replay", "--handler", "python handler.py", "--", "events.json"]);
});

test("one compiler", async (t) => {
    await t.cli(["-m", "replay", "--handler", "python compilers.py", "--", "events.json"]);
});

test("an archiver and a tool by name", async (t) => {
    await t.cli(["-m", "replay", "--handler", "python archivers.py", "--", "events.json"]);
});
