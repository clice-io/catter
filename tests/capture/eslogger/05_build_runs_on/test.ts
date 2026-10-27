import { test } from "@catter/tools/testing";

// eslogger tells catter of each command once it runs: a session that has
// ended stops none of them.
test(
    {
        except: { linux: "eslogger is macOS's", win32: "eslogger is macOS's" },
        needs: ["eslogger"],
        errors: ["CDB aborting after compiler command"],
    },
    async (t) => {
        await t.cli(
            "compile-commands --capture eslogger --abort-on-command-failure -o compile_commands.json -- make -k",
        );
        t.show("fine.o", t.disk.exists("fine.o"));
    },
);
