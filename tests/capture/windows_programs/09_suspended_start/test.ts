import { test } from "@catter/tools/testing";

// A program starts its child suspended to prepare it before it runs, as
// MSBuild's tracker and a debugger do: the process it gets is its program's,
// not one catter starts in its place, and runs only once resumed. A child a
// handler drops can still be prepared, and ends with 0 once resumed.
const windowsOnly = {
    except: {
        linux: "Windows processes and the way Windows starts a program",
        darwin: "Windows processes and the way Windows starts a program",
    },
};

test("run", windowsOnly, async (t) => {
    await t.setup("cl /nologo suspended.c");
    await t.run("suspended.exe");
    await t.cli("tree -a 0 -- suspended.exe");
});

test("dropped", windowsOnly, async (t) => {
    await t.setup("cl /nologo suspended.c");
    await t.cli(["run", "--handler", "python handler.py", "--", "suspended.exe"]);
});
