// Starting a file without execute permission fails to start it.
import { spawnSync } from "node:child_process";
import * as fs from "node:fs";

fs.writeFileSync("not-executable", "#!/bin/sh\necho ran\n", { mode: 0o644 });
const child = spawnSync("./not-executable", { encoding: "utf8" });
console.log(child.error === undefined ? `ran, exit ${child.status}: ${child.stdout.trim()}` : `failed to start: ${child.error.code}`);
