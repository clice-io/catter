import path from "node:path";
import { test } from "@catter/tools/testing";

// Under the wrapper backend, a shim runs a compile as the handler decides: as it is, not at all
// with exit code 0, modified, and once the handler aborts the build, not at all with exit code 1.
test(
    {
        except: { win32: "the case's compilers are scripts, which Windows runs no shim for" },
        errors: ["aborted the build at command 5"],
    },
    async (t) => {
        const env = {
            PATH: [path.join(t.root, "tools"), process.env["PATH"] ?? ""].join(path.delimiter),
        };
        await t.cli(
            [
                "run",
                "--capture",
                "wrapper",
                "--handler",
                "python handler.py",
                "--",
                "node",
                "build.mjs",
            ],
            {
                env,
            },
        );
    },
);
