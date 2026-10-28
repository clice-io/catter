import { test } from "@catter/tools/testing";

// A process of the build that is no longer a descendant of catter, as a
// daemon the build starts is not, has its commands seen all the same.
test(
    { except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" } },
    async (t) => {
        await t.cli("compile-commands --capture ptrace -o compile_commands.json -- sh build.sh");
        t.show("compile_commands.json", t.disk.cdb());
    },
);
