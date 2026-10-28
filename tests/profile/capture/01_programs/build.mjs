// Two programs one after the other, the second failing, then two side by side: each says it
// runs, and waits for its input to end, which ends once both run.
import { spawn, spawnSync } from "node:child_process";

spawnSync("node", ["-e", "0"]);
spawnSync("node", ["-e", "process.exitCode = 3"]);

const running = ["a", "b"].map((name) => {
  const child = spawn("node", ["wait.mjs", name], { stdio: ["pipe", "pipe", "inherit"] });
  const said = new Promise((resolve) => child.stdout.once("data", resolve));
  const ended = new Promise((resolve) => child.on("close", resolve));
  return { child, said, ended };
});
await Promise.all(running.map(({ said }) => said));
for (const { child } of running) {
  child.stdin.end();
}
await Promise.all(running.map(({ ended }) => ended));
