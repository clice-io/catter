import { test } from "@catter/tools/testing";

// A compile names a response file that is gone when catter reads it, as one a
// build removes once the compile has run may be under a backend that only
// observes the build: catter says so, as it does of a command it could not
// capture.
test("reported", async (t) => {
    await t.cli("compile-commands -o compile_commands.json --from events.json");
    t.show("compile_commands.json", t.disk.cdb());
});

test("aborts", { errors: ["names the response file `flags.rsp`"] }, async (t) => {
    await t.cli(
        "compile-commands --abort-on-capture-error -o compile_commands.json --from events.json",
    );
    t.show("compile_commands.json", t.disk.cdb());
});
