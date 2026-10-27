import { test } from "@catter/tools/testing";

// eslogger tells catter of each command once it runs, which no decision
// changes then: a handler that decides is refused before the build starts.
test(
    {
        except: { linux: "eslogger is macOS's", win32: "eslogger is macOS's" },
        errors: ["decides on the build's commands"],
    },
    async (t) => {
        await t.cli([
            "run",
            "--capture",
            "eslogger",
            "--handler",
            "python handler.py",
            "--",
            "node",
            "-e",
            "require('node:fs').writeFileSync('built', '')",
        ]);
        t.show("built", t.disk.exists("built"));
    },
);
