// Installs the catter under test in the directory the argument names, with
// the programs and hook libraries beside it, and runs a build with it.
import { copyFileSync, mkdirSync, readdirSync } from "node:fs";
import { spawnSync } from "node:child_process";
import * as path from "node:path";

const dir = process.argv[2];
const from = path.dirname(process.env.CATTER_EXECUTABLE);
mkdirSync(dir);
for (const name of readdirSync(from)) {
  if (/^(catter|catter-proxy)(\.exe)?$|^libcatter-hook-unix\.(so|dylib)$|\.dll$/.test(name)) {
    copyFileSync(path.join(from, name), path.join(dir, name));
  }
}
const catter = path.resolve(dir, path.basename(process.env.CATTER_EXECUTABLE));
const run = spawnSync(catter, ["tree", "-w", "1000", "--", "node", "build.mjs"], {
  stdio: "inherit",
});
process.exit(run.status ?? 1);
