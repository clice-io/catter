// Starts a program with a variable of 30000 characters, close to the limit
// of a variable on Windows.
import { spawnSync } from "node:child_process";

const child = spawnSync("node", ["child.mjs"], {
  encoding: "utf8",
  env: { ...process.env, CATTER_PROBE_BIG: "v".repeat(30000) },
});
process.stdout.write(child.stdout);
process.exitCode = child.status ?? 1;
