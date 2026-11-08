import { test } from "@catter/tools/testing";

// explain without a command to analyze, its help, an option it does not take, and a --help after
// --, which is the analyzed command's.
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
        await t.cli("explain -- clang --target=x86_64-linux-gnu -c a.c --help");
    },
);
