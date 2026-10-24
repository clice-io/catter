// Starts one program, which reaches catter through the session's pipe.
import { spawnSync } from "node:child_process";

const child = spawnSync("node", ["-e", "console.log('child ran')"], {
  encoding: "utf8",
});
process.stdout.write(child.stdout);
