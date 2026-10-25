import { test } from "@catter/tools/testing";

// Two catter sessions run at once, as a terminal and an editor or two CI jobs
// on one machine do: each sees exactly its own commands.
test(async (t) => {
    await Promise.all([t.cli("tree -- node build.mjs A"), t.cli("tree -- node build.mjs B")]);
});
