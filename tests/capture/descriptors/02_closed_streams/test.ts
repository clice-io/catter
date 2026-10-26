import { test } from "@catter/tools/testing";

// A program started without a standard stream gets none there under catter
// either: not catter's log, nor anything else catter or catter-proxy opened.
test(
    {
        except: {
            win32: "a closed descriptor is POSIX's: Windows gives a program an invalid handle (windows_programs/08_invalid_std_handle)",
        },
    },
    async (t) => {
        await t.setup("cc -o report report.c");
        await t.run("node build.mjs");
        await t.cli("tree -w 1000 -- node build.mjs");
        await t.run("node catter.mjs");
    },
);
