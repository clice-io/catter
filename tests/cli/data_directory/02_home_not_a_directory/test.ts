import { test } from "@catter/tools/testing";

// A data directory catter cannot create, as under a HOME a sandbox points at
// nothing, is reported by its path before the build starts.
test(
    { except: { win32: "the session's pipe is a named pipe on Windows, in no directory" } },
    async (t) => {
        await t.cli("tree -w 1000 -- node -e 0", {
            env: { CATTER_HOME: "", HOME: `${t.root}/home.txt` },
        });
    },
);
