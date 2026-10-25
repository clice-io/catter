import { test } from "@catter/tools/testing";

// A command line that names no subcommand, or one catter does not have, is reported with the
// subcommands, before any build starts.
test(async (t) => {
    await t.cli("no-such-subcommand -- node -e 0");
    await t.cli("-- node -e 0");
});
