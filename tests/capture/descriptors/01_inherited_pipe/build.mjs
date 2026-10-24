// A build tool hands its program a pipe beyond stdio, as make does with its
// jobserver; the program writes to it.
import { spawnSync } from "node:child_process";

const child = spawnSync("node", ["child.mjs"], {
  stdio: ["ignore", "pipe", "inherit", "pipe"],
  encoding: "utf8",
});
console.log(`through fd 3: ${JSON.stringify(child.output[3])}, exit ${child.status}`);
