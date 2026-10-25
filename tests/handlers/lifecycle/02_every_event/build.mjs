// Runs a program that fails, then one in a directory of its own.
import { spawnSync } from "node:child_process";
import { mkdirSync } from "node:fs";

console.log(`fails: exit ${spawnSync("node", ["-e", "process.exit(3)"]).status}`);
mkdirSync("sub", { recursive: true });
console.log(`in sub: exit ${spawnSync("node", ["-e", "0"], { cwd: "sub" }).status}`);
