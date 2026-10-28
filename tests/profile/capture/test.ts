import { test } from "@catter/tools/testing";

// A case's build.mjs starts programs the way a build tool does: each capture backend gives the
// times of the commands it captures, and the trace lays them out as they ran.
test("hook", async (t) => {
    await t.cli("profile -q -- node build.mjs");
    t.show("trace.json", t.disk.trace());
});

test(
    "ptrace",
    { except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" } },
    async (t) => {
        await t.cli("profile -q --capture ptrace -- node build.mjs");
        t.show("trace.json", t.disk.trace());
    },
);

test(
    "eslogger",
    {
        except: { linux: "eslogger is macOS's", win32: "eslogger is macOS's" },
        needs: ["eslogger"],
    },
    async (t) => {
        await t.cli("profile -q --capture eslogger -- node build.mjs");
        t.show("trace.json", t.disk.trace());
    },
);
