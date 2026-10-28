import { test } from "@catter/tools/testing";

const linuxOnly = {
    except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" },
};

// catter tells of each command once it runs: a session that has ended stops
// none of them.
test("aborted", { ...linuxOnly, errors: ["CDB aborting after compiler command"] }, async (t) => {
    await t.cli(
        "compile-commands --capture ptrace --abort-on-command-failure -o compile_commands.json -- make -k",
    );
    t.show("fine.o", t.disk.exists("fine.o"));
});

// A process the build leaves behind runs on once the build command has ended,
// which catter ends with, no longer traced.
test("outlived", linuxOnly, async (t) => {
    await t.cli("tree --capture ptrace -w 1000 -- sh outlive.sh");
    await t.run(["sh", "-c", "echo > go; cat done"]);
});
