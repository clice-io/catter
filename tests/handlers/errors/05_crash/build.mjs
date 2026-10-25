// Runs three programs, one after another, and prints how each ended.
import { spawnSync } from "node:child_process";

for (const tag of ["first", "second", "third"]) {
  const child = spawnSync("node", ["-e", "0", tag]);
  console.log(`${tag}: exit ${child.status}`);
}
