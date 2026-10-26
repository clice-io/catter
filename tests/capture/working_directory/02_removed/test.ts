import { test } from "@catter/tools/testing";

test(
    { except: { win32: "Windows does not let a directory a process runs in be removed" } },
    async (t) => {
        await t.run("node build.mjs");
        await t.cli("tree -w 1000 -- node build.mjs");
    },
);
