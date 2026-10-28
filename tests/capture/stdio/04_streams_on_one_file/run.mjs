// Runs build.mjs, then catter on it with the options it is given, each with stdout and stderr on one file,
// and prints the file.
import { spawnSync } from "node:child_process";
import { closeSync, openSync, readFileSync } from "node:fs";

const runs = {
  "node build.mjs": ["node", "build.mjs"],
  catter: [
    process.env.CATTER_EXECUTABLE,
    "tree",
    ...process.argv.slice(2),
    "-w",
    "1000",
    "--",
    "node",
    "build.mjs",
  ],
};
for (const [name, [program, ...args]] of Object.entries(runs)) {
  const output = openSync("output.txt", "w");
  const run = spawnSync(program, args, { stdio: ["ignore", output, output] });
  closeSync(output);
  console.log(`${name}, exit ${run.status}:`);
  process.stdout.write(readFileSync("output.txt", "utf8"));
}
