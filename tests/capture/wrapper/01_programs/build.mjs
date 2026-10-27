import { spawnSync } from "node:child_process";

// Runs the case's compilers as a build tool does: by name, as PATH finds them, and the one CC
// names by its path; and node, which no shim stands in for.
for (const [program, ...args] of [
    ["cc", "-c", "a.c"],
    ["c++", "-c", "b.cc"],
    ["ar", "rcs", "lib.a", "a.o"],
    [process.env.CC, "-c", "c.c"],
    ["node", "-e", "0"],
]) {
    spawnSync(program, args, { stdio: "inherit" });
}
