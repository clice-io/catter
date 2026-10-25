import { test } from "@catter/tools/testing";

// catter and the build write to one stdout: what --verbose says of a command
// comes before what the command prints.
test(async (t) => {
    await t.cli("compile-commands --verbose -o compile_commands.json -- node build.mjs");
});
