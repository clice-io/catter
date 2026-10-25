// Runs a program that fails, then one that does not, and prints how each
// ended.
import { spawnSync } from "node:child_process";

console.log(`fails: exit ${spawnSync("node", ["-e", "process.exit(3)"]).status}`);
console.log(`after it: exit ${spawnSync("node", ["-e", "0"]).status}`);
