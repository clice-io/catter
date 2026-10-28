import { test } from "@catter/tools/testing";

// sudo is set-user-ID root: the hook starts it as it is, but a program catter
// traces runs without the privileges of its file, and catter warns of it.
const options = {
    except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" },
    needs: ["sudo" as const],
};

test("hook", options, async (t) => {
    await t.cli("tree -w 1000 -- sudo -n true");
});

test("ptrace", options, async (t) => {
    await t.cli("tree --capture ptrace -w 1000 -- sudo -n true");
});
