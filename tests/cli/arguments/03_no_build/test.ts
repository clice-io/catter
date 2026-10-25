import { test } from "@catter/tools/testing";

// A command that runs a build is given none, or both a build command and a recorded build:
// either is reported before anything runs.
test(
    {
        errors: [
            "give the build command after --, or a recorded build with --from",
            "--from replays a recorded build in place of the build command after --",
        ],
    },
    async (t) => {
        await t.cli("tree");
        await t.cli("tree --from events.json -- node -e 0");
    },
);
