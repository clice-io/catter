import { test } from "@catter/tools/testing";

// A Go program built without cgo is static, and starts programs from
// whichever of its threads runs the goroutine: the hook misses the compiler
// it starts, the ptrace backend sees it.
const linuxOnly = {
    except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" },
    needs: ["go" as const],
};

test("hook", linuxOnly, async (t) => {
    await t.setup("go build -o run main.go", { env: { CGO_ENABLED: "0" } });
    await t.cli("compile-commands -o compile_commands.json -- ./run");
    t.show("compile_commands.json", t.disk.cdb());
});

test("ptrace", linuxOnly, async (t) => {
    await t.setup("go build -o run main.go", { env: { CGO_ENABLED: "0" } });
    await t.cli("compile-commands --capture ptrace -o compile_commands.json -- ./run");
    t.show("compile_commands.json", t.disk.cdb());
});
