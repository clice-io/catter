// Starts node under an argv[0] of its own choosing, as multi-call programs
// (clang++, busybox) are started to choose their mode.
import { spawnSync } from "node:child_process";

const child = spawnSync("node", ["child.mjs"], {
  argv0: "custom-argv0",
  encoding: "utf8",
});
process.stdout.write(child.stdout);
process.exitCode = child.status ?? 1;
