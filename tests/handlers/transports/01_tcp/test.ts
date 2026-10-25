import { test } from "@catter/tools/testing";

// A handler listening on a port serves the session of the catter that
// connects to it: the case's serve.py starts that catter itself, once it
// listens.
test(async (t) => {
    await t.run("python serve.py");
});
