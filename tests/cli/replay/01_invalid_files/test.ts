import { test } from "@catter/tools/testing";

// A replay file catter cannot play is reported before anything replays.
test({ errors: ["Unsupported replay file version", "Replay file is not valid"] }, async (t) => {
    await t.cli("tree --from version.json");
    await t.cli("tree --from missing_field.json");
    await t.cli("tree --from not_json.json");
});
