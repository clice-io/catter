// A chain of programs, each starting the next, to a leaf four levels down.
import { spawnSync } from "node:child_process";

const child = spawnSync("node", ["child.mjs", "3"], { encoding: "utf8" });
process.stdout.write(child.stdout);
process.exitCode = child.status ?? 1;
