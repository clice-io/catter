import { test } from "@catter/tools/testing";

// The launcher waits for the end of a pipe its helper holds to close, which
// the helper's exec of the program closes, before it gives the program its
// input: under catter too, nothing else holds that end.
test("posix", { except: { win32: "the POSIX functions" } }, async (t) => {
    await t.setup("cc -o launch launch.c");
    await t.run("./launch cat");
    await t.cli("tree -w 1000 -- ./launch cat");
});

const linuxOnly = {
    except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" },
};

test("ptrace", linuxOnly, async (t) => {
    await t.setup("cc -o launch launch.c");
    await t.cli("tree --capture ptrace -w 1000 -- ./launch cat");
});
