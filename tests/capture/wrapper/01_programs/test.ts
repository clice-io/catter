import path from "node:path";
import { test } from "@catter/tools/testing";

// build.mjs runs the case's compilers, programs of tools/ that print what they were run with,
// other/cc, which CC names, and tools/c++, which CXX names: run directly, then under the wrapper
// backend, whose shims stand on PATH and in CC's and CXX's place, beside the tools of
// CC's directory, and whose launcher CMake would run them through. Each compile is told once, by
// the program it runs.
test(
    { except: { win32: "the case's compilers are scripts, which Windows runs no shim for" } },
    async (t) => {
        const env = {
            PATH: [path.join(t.root, "tools"), process.env["PATH"] ?? ""].join(path.delimiter),
            CC: path.join(t.root, "other", "cc"),
            CXX: path.join(t.root, "tools", "c++"),
        };
        await t.run("node build.mjs", { env });
        await t.cli("tree -w 1000 --capture wrapper -- node build.mjs", { env });
    },
);
