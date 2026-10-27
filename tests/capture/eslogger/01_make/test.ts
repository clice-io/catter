import { test } from "@catter/tools/testing";

// Apple's make, the shell it runs recipes with and the compiler driver it
// finds as c++ run without the hook; eslogger sees each.
test(
    {
        except: { linux: "eslogger is macOS's", win32: "eslogger is macOS's" },
        needs: ["eslogger"],
    },
    async (t) => {
        await t.cli("compile-commands --capture eslogger -o compile_commands.json -- make");
        t.show("compile_commands.json", t.disk.cdb());
    },
);
