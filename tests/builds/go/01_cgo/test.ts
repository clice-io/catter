import { join } from "node:path";
import { test } from "@catter/tools/testing";

// go, a static program, runs cgo's C compiler with exec calls the hook never
// sees: the compiles report themselves, and the database has the package's C
// source. Go's own packages go to the cache first, so that the build under
// catter compiles the package alone.
test(
    "hook",
    {
        except: {
            darwin: "Go uses macOS's C library, whose exec the hook sees",
            win32: "the POSIX hook",
        },
    },
    async (t) => {
        // Go reads none of the user's settings (GOENV), and builds with the
        // case's C compiler.
        const env = {
            CC: "gcc",
            CGO_ENABLED: "1",
            GOCACHE: join(t.root, ".gocache"),
            GOENV: "off",
            GOTOOLCHAIN: "local",
        };
        await t.setup("go build -o hello .", { env });
        t.disk.edit("greet.c", { replace: `"hello"`, with: `"hello again"` });
        await t.cli("compile-commands -o compile_commands.json -- go build -o hello .", { env });
        t.show("compile_commands.json", t.disk.cdb());
        await t.run("./hello");
    },
);
