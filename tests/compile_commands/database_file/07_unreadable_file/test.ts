import { test } from "@catter/tools/testing";

// The database holds no JSON, as a save cut short would leave it: catter stops
// before the build runs, and leaves the file as it is.
test({ errors: ["CDB file is not valid JSON"] }, async (t) => {
    t.disk.write("compile_commands.json", '[{"directory": "/work", "file": ');
    await t.cli([
        "compile-commands",
        "-o",
        "compile_commands.json",
        "--",
        "node",
        "-e",
        "console.log('built')",
    ]);
    t.show("compile_commands.json", t.disk.read("compile_commands.json"));
});
