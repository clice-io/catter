/// The build of a session that has several steps, run one after another:
/// the first that fails ends it with its exit code.
///
///   node tools/ecosystem/steps.ts '<JSON list of argv lists>'

import { spawnSync } from "node:child_process";

const steps = JSON.parse(process.argv[2] ?? "[]") as string[][];
for (const [command = "", ...args] of steps) {
    const result = spawnSync(command, args, { stdio: "inherit" });
    if (result.error !== undefined) {
        console.error(`${command}: ${result.error.message}`);
        process.exit(127);
    }
    if (result.status !== 0) {
        process.exit(result.status ?? 1);
    }
}
