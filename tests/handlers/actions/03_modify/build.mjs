// Runs two programs, each with its tag in its arguments and its environment:
// the handler acts on the one with "act-on-me" in its arguments, and leaves
// the other alone.
import { spawnSync } from "node:child_process";

for (const tag of ["act-on-me", "leave-me"]) {
  const child = spawnSync("node", ["child.mjs", tag], {
    encoding: "utf8",
    env: { ...process.env, TAG: tag },
  });
  process.stdout.write(child.stdout);
  console.log(`${tag}: exit ${child.status}`);
}
