import path from "node:path";
import { test } from "@catter/tools/testing";

// PATH finds cc as a build cache's, cache/cc, which runs the cc it finds past its own directory:
// under the wrapper backend it finds the compiler, not the shim again, and the compile is told
// once.
test({ except: { win32: "the wrapper backend runs on Linux and macOS" } }, async (t) => {
    const env = {
        PATH: [path.join(t.root, "cache"), path.join(t.root, "tools"), process.env.PATH ?? ""].join(
            path.delimiter,
        ),
    };
    await t.run("node build.mjs", { env });
    await t.cli("tree -w 1000 --capture wrapper -- node build.mjs", { env });
});
