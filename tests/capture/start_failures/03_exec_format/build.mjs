// Starts each file with the functions of the exec family and posix_spawn, as
// build tools do: the system decides what starts, and how a start fails.
import { spawnSync } from "node:child_process";

const env = { ...process.env, PATH: `.:${process.env.PATH}` };
const starts = [
  ["execv", "./no-format"],
  ["posix_spawn", "./no-format"],
  ["posix_spawnp", "no-format"],
  ["execvp", "no-format"],
  ["execv", "./lost-interpreter"],
  ["posix_spawn", "./lost-interpreter"],
];
for (const [fn, file] of starts) {
  const run = spawnSync("./start", [fn, file], { env, encoding: "utf8" });
  console.log(`${fn} ${file}: ${run.stdout.trim()}`);
}
