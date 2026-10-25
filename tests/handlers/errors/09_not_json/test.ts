import { test } from "@catter/tools/testing";

// A framed message that is not JSON is reported as soon as it comes, rather
// than as an answer that never comes: --handler-timeout 0 would wait for it
// for good.
test({ errors: ["wrote what is not a protocol message"] }, async (t) => {
    await t.cli([
        "--handler",
        "python handler.py",
        "--handler-timeout",
        "0",
        "--",
        "node",
        "-e",
        "0",
    ]);
});
