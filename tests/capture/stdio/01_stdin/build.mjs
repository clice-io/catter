// Feeds a program 20000 lines through a pipe on its stdin.
import { spawnSync } from "node:child_process";

let input = "";
for (let i = 0; i < 20000; i++) {
  input += `line ${i}\n`;
}
const child = spawnSync("node", ["child.mjs"], { encoding: "utf8", input });
process.stdout.write(child.stdout);
process.exitCode = child.status ?? 1;
