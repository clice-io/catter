import { spawnSync } from "node:child_process";

// Runs cc, which PATH finds as the build cache's; then again with the environment a tool that
// keeps a build's PATH alone gives it, which no session reaches: uncaptured, it runs the compiler
// all the same.
spawnSync("cc", ["-c", "a.c"], { stdio: "inherit" });
spawnSync("cc", ["-c", "b.c"], { stdio: "inherit", env: { PATH: process.env["PATH"] } });
