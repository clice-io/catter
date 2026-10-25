import { test } from "@catter/tools/testing";

// A replay file catter cannot play is reported before anything replays.
test({ errors: ["Unsupported replay file version", "Replay file is not valid"] }, async (t) => {
    await t.cli("-m replay script::cmd-tree -- version.json");
    await t.cli("-m replay script::cmd-tree -- missing_field.json");
    await t.cli("-m replay script::cmd-tree -- not_json.json");
});
