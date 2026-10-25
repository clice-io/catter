import { test } from "@catter/tools/testing";

// An answer that is not an action catter can take ends the session with an
// error that names it: the command does not run.
test("unknown action", { errors: ["command 1"] }, async (t) => {
    await t.cli(["--handler", "python handler.py unknown", "--", "node", "-e", "0"]);
});

test("bad base64", { errors: ["command 1"] }, async (t) => {
    await t.cli(["--handler", "python handler.py base64", "--", "node", "-e", "0"]);
});
