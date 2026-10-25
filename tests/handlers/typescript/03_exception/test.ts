import { test } from "@catter/tools/testing";

// An exception in a callback of the TypeScript SDK answers catter with an
// error, which ends the session with an error that names it.
test({ errors: ["failed on command 1"] }, async (t) => {
    await t.cli(["--handler", "node handler.ts", "--", "node", "-e", "0"]);
});
