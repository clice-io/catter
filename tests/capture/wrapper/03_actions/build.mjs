import { spawnSync } from "node:child_process";

// Runs a compile the handler leaves as it is, one it drops, one it modifies and one it aborts the
// build at, then one more, and prints how each ended.
for (const source of ["keep.c", "drop.c", "modify.c", "abort.c", "after.c"]) {
    const result = spawnSync("cc", ["-c", source], { stdio: "inherit" });
    console.log(`${source}: exit ${result.status}`);
}
