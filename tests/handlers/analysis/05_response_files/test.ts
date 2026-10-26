import { test } from "@catter/tools/testing";

// The case's compiles take arguments from response files, nested ones too, and
// one names a response file that is not there. handler.py prints what catter
// reads of them.
test("analysis", async (t) => {
    await t.cli(["run", "--handler", "python handler.py", "--from", "events.json"]);
});

// decider.py edits the commands: a response file whose arguments no edit takes
// out stays as the command names it, and one whose arguments an edit takes out
// gives the rest in its place. observer.py, after it in the chain, prints each
// command as edited.
test("edits", async (t) => {
    await t.cli([
        "run",
        "--handler",
        "python decider.py",
        "--handler",
        "python observer.py",
        "--from",
        "events.json",
    ]);
});
