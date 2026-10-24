import { test } from "@catter/tools/testing";

// A script imports a module beside it by relative path.
test(async (t) => {
    await t.cli("script.js -- node -e 0");
});
