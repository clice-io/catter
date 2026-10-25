import { test } from "@catter/tools/testing";

test(
    {
        except: {
            win32: "Windows hands a program no descriptor beyond stdio by number",
        },
    },
    async (t) => {
        await t.run("node build.mjs");
        await t.cli("tree -w 1000 -- node build.mjs");
    },
);
