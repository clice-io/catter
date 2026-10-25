import { test } from "@catter/tools/testing";

// The build itself runs with a variable the program it starts must not see.
test(async (t) => {
    const env = { CATTER_PROBE_REMOVED: "the build's own" };
    await t.run("node build.mjs", { env });
    await t.cli("tree -w 1000 -- node build.mjs", { env });
});
