import { test } from "@catter/tools/testing";

// A handler runs in catter's working directory: the case's handler.py works
// on files there once the build is done, and what a program it starts
// prints shows on stderr.
test(async (t) => {
    await t.cli(["run", "--handler", "python handler.py", "--", "node", "-e", "0"]);
});
