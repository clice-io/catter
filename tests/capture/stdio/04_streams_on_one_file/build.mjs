// Says what its stdout and stderr are, then starts child.mjs on them, as a
// build tool starts its commands.
import { spawnSync } from "node:child_process";
import { fstatSync, writeSync } from "node:fs";

const kind = (fd) => {
  const stat = fstatSync(fd);
  return stat.isFile() ? "a file" : stat.isFIFO() ? "a pipe" : "neither file nor pipe";
};
writeSync(1, `build: stdout is ${kind(1)}, stderr is ${kind(2)}\n`);
spawnSync("node", ["child.mjs"], { stdio: "inherit" });
