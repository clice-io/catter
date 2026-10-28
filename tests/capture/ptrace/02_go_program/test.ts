import { test } from "@catter/tools/testing";

// A Go program built without cgo is static, and starts programs from
// whichever of its threads runs the goroutine: the hook misses the exec, and
// the compiler reports itself once the hook loads into it; the ptrace backend
// sees the exec.
const elsewhere =
    "a Go program starts programs there through the system's library, which has the hook";

test("hook", { except: { darwin: elsewhere, win32: elsewhere }, needs: ["go"] }, async (t) => {
    await t.setup("go build -o run main.go", { env: { CGO_ENABLED: "0" } });
    await t.cli("compile-commands -o compile_commands.json -- ./run");
    t.show("compile_commands.json", t.disk.cdb());
});

test(
    "ptrace",
    {
        except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" },
        needs: ["go"],
    },
    async (t) => {
        await t.setup("go build -o run main.go", { env: { CGO_ENABLED: "0" } });
        await t.cli("compile-commands --capture ptrace -o compile_commands.json -- ./run");
        t.show("compile_commands.json", t.disk.cdb());
    },
);
