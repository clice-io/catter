// Runs three programs and prints what each received.
import { spawnSync } from "node:child_process";

for (const tag of ["drop-me", "modify-me", "leave-me"]) {
  const child = spawnSync("node", ["child.mjs", tag], { encoding: "utf8" });
  process.stdout.write(child.stdout);
  console.log(`${tag}: exit ${child.status}`);
}
