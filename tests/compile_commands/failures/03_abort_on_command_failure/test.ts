import { test } from "@catter/tools/testing";

// A compile fails: --abort-on-command-failure ends the run there, and
// --save-on-failure keeps the entries so far.
test({ errors: ["CDB aborting after compiler command 1 exited with code 2"] }, async (t) => {
    await t.cli(
        "compile-commands --abort-on-command-failure --save-on-failure -o compile_commands.json --from events.json",
    );
    t.show("compile_commands.json", t.disk.cdb());
});
