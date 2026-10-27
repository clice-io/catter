import path from "node:path";
import { test } from "@catter/tools/testing";

// first/ has g++, second/ c++ and g++, and other/, on PATH twice, no compiler: a build tool that
// takes the first directory on PATH with either name runs first/g++, and one that looks for c++
// by its name second/c++, not the c++ of the directory it runs in, which PATH does not name, run
// directly as under the wrapper backend, whose shims keep the order of PATH's directories.
test(
    { except: { win32: "the case's compilers are scripts, which Windows runs no shim for" } },
    async (t) => {
        const env = {
            PATH: [
                path.join(t.root, "other"),
                path.join(t.root, "other"),
                path.join(t.root, "first"),
                path.join(t.root, "second"),
                process.env["PATH"] ?? "",
            ].join(path.delimiter),
        };
        await t.run("node build.mjs", { env });
        await t.cli("tree -w 1000 --capture wrapper -- node build.mjs", { env });
    },
);
