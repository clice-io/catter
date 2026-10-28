import { test } from "@catter/tools/testing";

// The build command names a program that does not exist: it does not start,
// catter tells why, and exits non-zero.
test("hook", { errors: ["spawn failed"] }, async (t) => {
    await t.cli("tree -w 1000 -- catter-test-no-such-program");
});

const linuxOnly = {
    except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" },
};

test("ptrace", linuxOnly, async (t) => {
    await t.cli("tree --capture ptrace -w 1000 -- catter-test-no-such-program");
});
