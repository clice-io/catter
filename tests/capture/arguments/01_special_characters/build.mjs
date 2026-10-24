// Starts a program the way a build tool starts a compiler, with no shell in
// between, and prints what it received.
import { spawnSync } from "node:child_process";

const args = [
  "plain",
  "with space",
  'quote"inside',
  "back\\slash",
  "trailing\\",
  "two\\\\",
  "",
  "unicode-中文-é",
  "tab\there",
  "a&b|c;d<e>f",
  "%PATH%",
  "$HOME",
  "*",
  "--flag=value with space",
  "'single'",
];
const child = spawnSync("node", ["child.mjs", ...args], { encoding: "utf8" });
process.stdout.write(child.stdout);
process.exitCode = child.status ?? 1;
