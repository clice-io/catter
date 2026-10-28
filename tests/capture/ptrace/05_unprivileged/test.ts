import { test } from "@catter/tools/testing";

// sudo is set-user-ID root, but a program catter traces runs without the
// privileges of its file: sudo fails, and catter warns of it.
test(
    {
        except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" },
        needs: ["sudo"],
    },
    async (t) => {
        await t.cli("tree --capture ptrace -w 1000 -- sudo -n true");
    },
);
