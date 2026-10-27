import { spawnSync } from "node:child_process";

// Runs the case's compilers as a build tool does: by name, as PATH finds them, and the one CC
// names by its path; as CMake runs them, through the launcher CMAKE_C_COMPILER_LAUNCHER names
// when there is one; and node, which no shim stands in for.
const launcher = process.env["CMAKE_C_COMPILER_LAUNCHER"];
const launched = launcher === undefined ? [] : [launcher];
for (const [program, ...args] of [
    ["cc", "-c", "a.c"],
    ["c++", "-c", "b.cc"],
    ["ar", "rcs", "lib.a", "a.o"],
    [process.env["CC"], "-c", "c.c"],
    [...launched, process.env["CC"], "-c", "d.c"],
    [...launched, "cc", "-c", "e.c"],
    ["node", "-e", "0"],
]) {
    spawnSync(program, args, { stdio: "inherit" });
}
