// Prints a line before and after it starts child.mjs, which prints one too.
import { spawnSync } from "node:child_process";
import { writeSync } from "node:fs";

writeSync(1, "build: starting child.mjs\n");
spawnSync("node", ["child.mjs"], { stdio: "inherit" });
writeSync(1, "build: child.mjs is done\n");
