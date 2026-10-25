import { test } from "@catter/tools/testing";

// inspect without a command to analyze, its help, and an option it does not take.
test(
    {
        errors: [
            "inspect: give the command to analyze after --",
            "inspect: unknown option '--bogus'",
        ],
    },
    async (t) => {
        await t.cli("inspect --");
        await t.cli("inspect --help -- gcc -c a.c");
        await t.cli("inspect --bogus -- gcc -c a.c");
    },
);
