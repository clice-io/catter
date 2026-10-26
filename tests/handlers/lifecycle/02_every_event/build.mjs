// Runs a program that fails, one that aborts, then one in a directory of its
// own.
import { spawnSync } from "node:child_process";
import { mkdirSync } from "node:fs";

console.log(`fails: exit ${spawnSync("node", ["-e", "process.exit(3)"]).status}`);
spawnSync("node", ["-e", "process.abort()"]);
console.log("aborts");
mkdirSync("sub", { recursive: true });
console.log(`in sub: exit ${spawnSync("node", ["-e", "0"], { cwd: "sub" }).status}`);
