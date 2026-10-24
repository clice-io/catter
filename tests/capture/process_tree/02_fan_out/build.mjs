// 32 programs started at once, as a parallel build does; each reports its
// output and exit code.
import { spawn } from "node:child_process";

const runs = Array.from(
  { length: 32 },
  (_, i) =>
    new Promise((resolve) => {
      const child = spawn("node", ["-e", `console.log("n${i}")`]);
      let out = "";
      child.stdout.on("data", (chunk) => (out += chunk));
      child.on("close", (code) => resolve(`${out.trim()} exit ${code}`));
    }),
);
for (const line of await Promise.all(runs)) {
  console.log(line);
}
