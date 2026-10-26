// A build runs a script, which the system starts with the interpreter its
// #! line names: on macOS Apple's /bin/sh.
import { spawnSync } from "node:child_process";
import * as fs from "node:fs";

fs.writeFileSync("script.sh", "#!/bin/sh\necho the script ran\n", { mode: 0o755 });
const child = spawnSync("./script.sh", { stdio: ["ignore", "pipe", "inherit"], encoding: "utf8" });
console.log(`${child.stdout.trim()}, exit ${child.status}`);
