import { test } from "@catter/tools/testing";

// script::inspect without a command to analyze, and its help.
test({ errors: ["inspect: give the command to analyze after --"] }, async (t) => {
    await t.cli("script::inspect --");
    await t.cli("script::inspect --help -- gcc -c a.c");
});
