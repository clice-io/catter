// Starts a program in a directory whose name has spaces and non-ASCII
// characters.
import { spawnSync } from "node:child_process";
import * as fs from "node:fs";
import * as path from "node:path";

const dir = path.resolve("with space 中文");
fs.mkdirSync(dir, { recursive: true });
const child = spawnSync("node", [path.resolve("child.mjs")], {
  cwd: dir,
  encoding: "utf8",
});
process.stdout.write(child.stdout);
process.exitCode = child.status ?? 1;
