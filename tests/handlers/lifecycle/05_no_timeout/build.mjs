// Runs a program the handler acts on, and prints what it received.
import { spawnSync } from "node:child_process";

const child = spawnSync("node", ["child.mjs", "act-on-me"], { encoding: "utf8" });
process.stdout.write(child.stdout);
