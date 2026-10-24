import { test } from "@catter/tools/testing";

// The help of script::cdb, options it does not take together, and a second path: none runs
// the build nor writes a database.
test(
    {
        errors: [
            "cdb: --append and --replace cannot be used together",
            "cdb: unexpected input argument b.json",
        ],
    },
    async (t) => {
        await t.cli("script::cdb --help -- node -e 0");
        await t.cli("script::cdb --append --replace -- node -e 0");
        await t.cli("script::cdb a.json b.json -- node -e 0");
        t.show("compile_commands.json", t.disk.cdb("build/compile_commands.json"));
        t.show("a.json", t.disk.cdb("a.json"));
        t.show("b.json", t.disk.cdb("b.json"));
    },
);
