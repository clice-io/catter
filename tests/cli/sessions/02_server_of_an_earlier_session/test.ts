import { test } from "@catter/tools/testing";

// A server one session started serves the build of the next: the session it
// was started in is gone, so what it runs runs as it would without catter,
// uncaptured.
test(async (t) => {
    await t.cli("tree -w 1000 -- node start.mjs");
    await t.cli("tree -w 1000 -- node client.mjs");
});
