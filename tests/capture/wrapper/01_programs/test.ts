import path from "node:path";
import { test } from "@catter/tools/testing";

// build.mjs runs the case's compilers, programs of tools/ that print what they were run with,
// and other/cc, which CC names: run directly, then under the wrapper backend, whose shims stand
// first on PATH and in CC's place, and whose launcher CMake would run them through. Each compile
// is told once, by the program it runs.
test({ except: { win32: "the wrapper backend runs on Linux and macOS" } }, async (t) => {
    const env = {
        PATH: [path.join(t.root, "tools"), process.env["PATH"] ?? ""].join(path.delimiter),
        CC: path.join(t.root, "other", "cc"),
    };
    await t.run("node build.mjs", { env });
    await t.cli("tree -w 1000 --capture wrapper -- node build.mjs", { env });
});
