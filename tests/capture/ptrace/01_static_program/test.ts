import { test } from "@catter/tools/testing";

// A static program starts the compiler: the hook, which only a dynamic
// program loads, misses it; the ptrace backend sees it.
test(
    "hook",
    {
        except: {
            darwin: "macOS links no program statically",
            win32: "Windows injects the hook into a static program too",
        },
    },
    async (t) => {
        await t.setup("cc -static -o run run.c");
        await t.cli("compile-commands -o compile_commands.json -- ./run");
        t.show("compile_commands.json", t.disk.cdb());
    },
);

const linuxOnly = {
    except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" },
};

test("ptrace", linuxOnly, async (t) => {
    await t.setup("cc -static -o run run.c");
    await t.cli("compile-commands --capture ptrace -o compile_commands.json -- ./run");
    t.show("compile_commands.json", t.disk.cdb());
});
