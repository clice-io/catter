import { test } from "@catter/tools/testing";

// The case's script.js works on files once the build is done.
test(async (t) => {
    await t.cli("script.js -- node -e 0");
});
