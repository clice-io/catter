import { test } from "@catter/tools/testing";

// script::inspect without a command to analyze, its help, and an option it does not take.
test(
    {
        errors: [
            "inspect: give the command to analyze after --",
            "inspect: unknown option '--bogus'",
        ],
    },
    async (t) => {
        await t.cli("script::inspect --");
        await t.cli("script::inspect --help -- gcc -c a.c");
        await t.cli("script::inspect --bogus -- gcc -c a.c");
    },
);
