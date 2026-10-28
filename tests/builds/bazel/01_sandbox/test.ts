import { test } from "@catter/tools/testing";

// Bazel runs the compile in a sandbox of symlinks to its execution root, which
// it removes once the compile has run: the entry is of the execution root.
test("hook", { except: { win32: "Bazel runs no action in a sandbox on Windows" } }, async (t) => {
    // Bazelisk downloads the release on its first run, and says so.
    await t.setup("bazel --version");
    await t.cli(
        "compile-commands -o compile_commands.json -- bazel --output_base=out build //:main",
    );
    t.show("compile_commands.json", t.disk.cdb());
});

// A Bazel server the build starts runs on once the build has ended, untraced.
test(
    "ptrace",
    { except: { darwin: "the ptrace backend is Linux's", win32: "the ptrace backend is Linux's" } },
    async (t) => {
        await t.setup("bazel --version");
        await t.cli(
            "compile-commands --capture ptrace -o compile_commands.json -- bazel --output_base=out build //:main",
        );
        t.show("compile_commands.json", t.disk.cdb());
    },
);
