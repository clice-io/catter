// Two programs one after the other, the second failing, then two side by side: each says it
// runs, and waits for its input to end, which ends once both run; the second starts once the
// first runs.
import { spawn, spawnSync } from "node:child_process";

spawnSync("node", ["-e", "0"]);
spawnSync("node", ["-e", "process.exitCode = 3"]);

const running = [];
for (const name of ["a", "b"]) {
  const child = spawn("node", ["wait.mjs", name], { stdio: ["pipe", "pipe", "inherit"] });
  const ended = new Promise((resolve) => child.on("close", resolve));
  await new Promise((resolve) => child.stdout.once("data", resolve));
  running.push({ child, ended });
}
for (const { child } of running) {
  child.stdin.end();
}
await Promise.all(running.map(({ ended }) => ended));
