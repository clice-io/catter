import { test, type TestContext } from "@catter/tools/testing";

// eslogger and ptrace tell catter of each command once it runs, which no
// decision changes then: a handler that decides is refused before the build
// starts.
async function decide(t: TestContext, backend: string): Promise<void> {
    await t.cli([
        "run",
        "--capture",
        backend,
        "--handler",
        "python handler.py",
        "--",
        "node",
        "-e",
        "require('node:fs').writeFileSync('built', '')",
    ]);
    t.show("built", t.disk.exists("built"));
}

test(
    "eslogger",
    {
        except: { linux: "eslogger is macOS's", win32: "eslogger is macOS's" },
        errors: ["decides on the build's commands"],
    },
    (t) => decide(t, "eslogger"),
);

test(
    "ptrace",
    {
        except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" },
        errors: ["decides on the build's commands"],
    },
    (t) => decide(t, "ptrace"),
);
