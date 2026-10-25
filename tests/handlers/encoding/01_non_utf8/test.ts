import { test } from "@catter/tools/testing";

// An argument that is not UTF-8 reaches a handler and, handed back in a
// modified command, the program, byte for byte. In the message of a command
// catter could not capture, and in what a handler sends back built from it,
// its bytes come as U+FFFD.
test(
    "modify",
    {
        except: {
            win32: "Windows passes arguments as UTF-16, which holds no bytes that are not UTF-8",
        },
        errors: ["Process `./broken` spawn failed"],
    },
    async (t) => {
        await t.cli(["--handler", "python handler.py", "--", "python", "build.py"]);
    },
);

test(
    "abort",
    {
        except: {
            win32: "Windows passes arguments as UTF-16, which holds no bytes that are not UTF-8",
        },
        errors: ["aborted the build at command 2"],
    },
    async (t) => {
        await t.cli(["--handler", "python abort.py", "--", "python", "build.py"]);
    },
);
