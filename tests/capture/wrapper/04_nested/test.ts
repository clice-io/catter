import { test } from "@catter/tools/testing";

// catter run in a build under the wrapper backend, whose environment names that session's shims,
// says so before it runs the wrapper backend, whose shims would meet that session's.
test(
    {
        errors: ["catter runs in a build under the wrapper backend"],
    },
    async (t) => {
        await t.cli("tree --capture wrapper -- node -e 0", {
            env: { __key_catter_shims_v1: `${t.root}/shims` },
        });
    },
);
