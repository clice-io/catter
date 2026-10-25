import { test } from "@catter/tools/testing";

// An argument that is not UTF-8 reaches the database with U+FFFD for its
// bytes: JSON holds UTF-8 only.
test(
    {
        except: {
            win32: "Windows passes arguments as UTF-16, which holds no bytes that are not UTF-8",
        },
    },
    async (t) => {
        await t.cli("compile-commands -o compile_commands.json -- python build.py");
        t.show("compile_commands.json", t.disk.cdb());
    },
);
