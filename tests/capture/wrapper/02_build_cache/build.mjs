import { spawnSync } from "node:child_process";

// Runs cc, which PATH finds as the build cache's.
spawnSync("cc", ["-c", "a.c"], { stdio: "inherit" });
