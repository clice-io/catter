import { test } from "@catter/tools/testing";

// A filter catter cannot take ends the session before the build starts, with
// an error that names what is wrong with it.
test({ errors: ["a filter catter cannot take"] }, async (t) => {
    await t.cli(["--handler", "python handler.py", "--", "node", "-e", "0"]);
});
