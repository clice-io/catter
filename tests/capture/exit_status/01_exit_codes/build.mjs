// The exit code of every program reaches the build tool that started it.
import { spawnSync } from "node:child_process";

for (const code of [0, 1, 3, 255]) {
  const child = spawnSync("node", ["-e", `process.exit(${code})`]);
  console.log(`exit(${code}) -> ${child.status}`);
}
