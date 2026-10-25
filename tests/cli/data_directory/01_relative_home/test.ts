import { test } from "@catter/tools/testing";

// A relative CATTER_HOME is refused: the build's processes start in other
// directories, where it names another one.
test(async (t) => {
    await t.cli("-d sub cmd-tree -w 1000 -- node build.mjs", {
        env: { CATTER_HOME: "home" },
    });
    t.show("home", t.disk.exists("home"));
});
