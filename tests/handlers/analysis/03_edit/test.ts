import { test } from "@catter/tools/testing";

// The case's decider.py edits the arguments of each command: it takes out
// options by name and value, and adds flags before the first input, or what
// ends the options, and after the program. observer.py, after it in the
// chain, sees each command as edited: the arguments no edit takes out as they
// were written, the added flags as decider.py wrote them.
test(async (t) => {
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
