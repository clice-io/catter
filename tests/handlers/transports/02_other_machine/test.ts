import { test } from "@catter/tools/testing";

// catter connects to a handler on another machine only when told it may:
// such a handler decides on the build.
test({ errors: ["is not on this machine"] }, async (t) => {
    await t.cli(["run", "--connect", "192.0.2.1:9000", "--", "node", "-e", "0"]);
});
