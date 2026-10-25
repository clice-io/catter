import { test } from "@catter/tools/testing";

// A framed message the protocol does not allow from a handler, which answers
// catter's requests and asks its own, is reported as soon as it comes, rather
// than as an answer that never comes: --handler-timeout 0 would wait for it
// for good.
test("not JSON", { errors: ["wrote what the protocol does not allow"] }, async (t) => {
    await t.cli([
        "--handler",
        "python not_json.py",
        "--handler-timeout",
        "0",
        "--",
        "node",
        "-e",
        "0",
    ]);
});

test("a notification", { errors: ["wrote what the protocol does not allow"] }, async (t) => {
    await t.cli([
        "--handler",
        "python notification.py",
        "--handler-timeout",
        "0",
        "--",
        "node",
        "-e",
        "0",
    ]);
});
