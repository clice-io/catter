import { test } from "@catter/tools/testing";

// The case's handler.py prints what each callback receives: under the hook,
// under eslogger, and under replay, with a command catter could not capture.
test("hook", async (t) => {
    await t.cli(["run", "--handler", "python handler.py", "--", "node", "build.mjs"]);
});

test(
    "eslogger",
    {
        except: { linux: "eslogger is macOS's", win32: "eslogger is macOS's" },
        needs: ["eslogger"],
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
            "build.mjs",
        ]);
    },
);

test("replay", async (t) => {
    await t.cli(["run", "--handler", "python handler.py", "--from", "events.json"]);
});
