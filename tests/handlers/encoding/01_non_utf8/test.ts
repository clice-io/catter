import { test } from "@catter/tools/testing";

// An argument that is not UTF-8 reaches a handler and, handed back in a
// modified command, the program, byte for byte.
test(
    {
        except: {
            win32: "Windows passes arguments as UTF-16, which holds no bytes that are not UTF-8",
        },
    },
    async (t) => {
        await t.cli(["--handler", "python handler.py", "--", "python", "build.py"]);
    },
);
