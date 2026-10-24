// Runs ten programs one after another, each tagged with the build's own tag
// (the first argument), so that a session that sees another's commands shows
// them.
import { spawnSync } from "node:child_process";

const tag = process.argv[2];
for (let i = 0; i < 10; i++) {
  spawnSync("node", ["-e", "0", `${tag}${i}`]);
}
console.log(`build ${tag} done`);
