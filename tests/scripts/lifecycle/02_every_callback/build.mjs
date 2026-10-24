// Runs one program and relays what it printed on stderr, apart from the
// script's lines on stdout.
import { spawnSync } from "node:child_process";
import { mkdirSync } from "node:fs";

mkdirSync("sub", { recursive: true });
const child = spawnSync("node", ["child.mjs", ...process.argv.slice(2)], { encoding: "utf8" });
process.stderr.write(child.stdout);
process.stderr.write(`child: exit ${child.status}\n`);
