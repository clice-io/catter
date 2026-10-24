/// Every case under tests/: each registered test a vitest test, and once
/// they all ran, the case's output against its snap.toml. CATTER_CASES=<path
/// prefix> runs the cases under it, CATTER_CASES_SHARD=i/n one of n shards.

import { afterAll, describe, test } from "vitest";
import { loadCases, shard } from "../cases/case.ts";
import { runRegistered } from "../cases/context.ts";
import {
    checkOutput,
    hostPlatform,
    PLATFORMS,
    referenceOf,
    type Blocks,
    type TestResult,
} from "../cases/output.ts";
import { CASES_DIR } from "../cases/project.ts";
import { loadScript, nameOf } from "../cases/script.ts";

const prefix = process.env["CATTER_CASES"] ?? "";
const under = loadCases(CASES_DIR).filter((c) => c.id.startsWith(prefix));
const spec = process.env["CATTER_CASES_SHARD"];
const cases = spec === undefined ? under : shard(under, spec);
const platform = hostPlatform();

const notApplicable: string[] = [];

for (const c of cases) {
    const registered = await loadScript(c.script);
    const ran = registered.filter((entry) => {
        const why = entry.options.except?.[platform];
        if (why !== undefined) {
            notApplicable.push(`${c.id} > ${nameOf(entry)}: ${why}`);
        }
        return why === undefined;
    });
    // A suite with no test fails in vitest.
    if (ran.length === 0) {
        continue;
    }
    describe(c.id, () => {
        const results = new Map<string, Blocks>();
        for (const entry of ran) {
            test.concurrent(nameOf(entry), async () => {
                results.set(nameOf(entry), await runRegistered(c, entry));
            });
        }
        afterAll(() => {
            if (ran.some((entry) => !results.has(nameOf(entry)))) {
                return;
            }
            checkOutput(
                c,
                registered.map((entry): TestResult => ({
                    name: nameOf(entry),
                    reference: referenceOf(entry.options.except ?? {}),
                    blocks: results.get(nameOf(entry)),
                    excepted: PLATFORMS.filter(
                        (each) => entry.options.except?.[each] !== undefined,
                    ),
                })),
                platform,
            );
        });
    });
}

afterAll(() => {
    if (notApplicable.length > 0) {
        console.log(`not applicable on ${platform}:\n  ${notApplicable.join("\n  ")}`);
    }
});
