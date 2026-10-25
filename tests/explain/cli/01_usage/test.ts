import { test } from "@catter/tools/testing";

// explain without a command to analyze, its help, and an option it does not take.
test(
    {
        errors: [
            "explain: give the command to analyze after --",
            "explain: unknown option '--bogus'",
        ],
    },
    async (t) => {
        await t.cli("explain --");
        await t.cli("explain --help -- gcc -c a.c");
        await t.cli("explain --bogus -- gcc -c a.c");
    },
);
