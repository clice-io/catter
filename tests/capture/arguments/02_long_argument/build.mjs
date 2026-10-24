// Starts a program with one argument of 30000 characters, close to the
// 32767-character limit of a command line on Windows.
import { spawnSync } from "node:child_process";

const child = spawnSync("node", ["child.mjs", "x".repeat(30000)], {
  encoding: "utf8",
});
process.stdout.write(child.stdout);
process.exitCode = child.status ?? 1;
