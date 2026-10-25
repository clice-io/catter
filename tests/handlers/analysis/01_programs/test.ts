import { test } from "@catter/tools/testing";

// What catter makes of the commands of a compiler of each dialect, an
// archiver and a tool, and of those whose arguments it cannot read: every
// argument by its option and as written, the files read, each source in its
// language, and the files written.
test(async (t) => {
    await t.cli(["run", "--handler", "python handler.py", "--from", "events.json"]);
});
