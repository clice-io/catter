// Runs a program with a variable of its own, and prints what it saw.
import { spawnSync } from "node:child_process";
import { mkdirSync } from "node:fs";

mkdirSync("sub", { recursive: true });
const child = spawnSync(
  "node",
  [
    "-e",
    "console.log(require('node:path').basename(process.cwd()), process.env.CASE_VAR, process.env.CASE_ADDED)",
    "child",
  ],
  { encoding: "utf8", env: { ...process.env, CASE_VAR: "set-by-build" } },
);
process.stdout.write(child.stdout);
