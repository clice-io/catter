import { test } from "@catter/tools/testing";

// A handler command quotes a path with a space, named past ASCII: catter
// splits it as a POSIX shell does, and Windows as its programs do.
test(async (t) => {
    await t.cli(["run", "--handler", 'python "中文 handler.py"', "--", "node", "-e", "0"]);
});
