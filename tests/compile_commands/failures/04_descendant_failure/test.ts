import { test } from "@catter/tools/testing";

// A program the compiler runs fails: it is not a command of the build, so
// --abort-on-command-failure goes on. Commands 3 and 4 name each other as parent.
test(async (t) => {
    await t.cli(
        "compile-commands --abort-on-command-failure -o compile_commands.json --from events.json",
    );
    t.show("compile_commands.json", t.disk.cdb());
});
