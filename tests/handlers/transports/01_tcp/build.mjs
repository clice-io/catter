// Runs two programs: the handler acts on the one with "act-on-me" in its
// arguments, and leaves the other alone.
import { spawnSync } from "node:child_process";

for (const tag of ["act-on-me", "leave-me"]) {
  const child = spawnSync("node", ["child.mjs", tag], { encoding: "utf8" });
  process.stdout.write(child.stdout);
  console.log(`${tag}: exit ${child.status}`);
}
