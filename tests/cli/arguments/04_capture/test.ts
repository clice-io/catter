import { test } from "@catter/tools/testing";

// --capture names the backend that captures the build, the hook unless it names another. A
// recorded build, which nothing captures, takes none, and a name no backend has is reported
// before anything runs.
test(
    {
        errors: [
            "`nope` is not a capture backend: give hook",
            "--from replays a recorded build, which nothing captures",
        ],
    },
    async (t) => {
        await t.cli("tree --capture hook -- node -e 0");
        await t.cli("tree --capture nope -- node -e 0");
        await t.cli("tree --capture hook --from events.json");
    },
);
