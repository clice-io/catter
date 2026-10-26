import { test } from "@catter/tools/testing";

// Each entry is of one command: two compiles that write the same file, and
// one source compiled from two directories, each named from its own.
test(async (t) => {
    await t.cli("compile-commands -o compile_commands.json --from events.json");
    t.show("compile_commands.json", t.disk.cdb());
});
