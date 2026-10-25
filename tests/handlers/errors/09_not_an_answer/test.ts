import { test } from "@catter/tools/testing";

// A framed message that is not an answer to one of catter's requests is
// reported as soon as it comes, rather than as an answer that never comes:
// --handler-timeout 0 would wait for it for good.
test("not JSON", { errors: ["wrote what is not an answer"] }, async (t) => {
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

test("a request", { errors: ["wrote what is not an answer"] }, async (t) => {
    await t.cli([
        "--handler",
        "python request.py",
        "--handler-timeout",
        "0",
        "--",
        "node",
        "-e",
        "0",
    ]);
});
