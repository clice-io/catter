import { test } from "@catter/tools/testing";

// Bazel runs the compile in a sandbox of symlinks to its execution root, which
// it removes once the compile has run: the entry is of the execution root.
test(
    "hook",
    { needs: ["bazel"], except: { win32: "Bazel runs no action in a sandbox on Windows" } },
    async (t) => {
        await t.cli(
            "compile-commands -o compile_commands.json -- bazel --output_base=out build //:main",
        );
        t.show("compile_commands.json", t.disk.cdb());
    },
);
