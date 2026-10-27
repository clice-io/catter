import { test } from "@catter/tools/testing";

test(
    {
        except: { linux: "eslogger is macOS's", win32: "eslogger is macOS's" },
        needs: ["eslogger"],
    },
    async (t) => {
        await t.cli("tree --capture eslogger -w 1000 -- make");
    },
);
