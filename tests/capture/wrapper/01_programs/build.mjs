import { spawnSync } from "node:child_process";
import path from "node:path";

// Runs the case's compilers as a build tool does: by name, as PATH finds them, and the ones CC
// and CXX name by their paths, CXX the one PATH finds first; a tool beside CC's compiler, as a
// build tool finds the tools that go with a compiler; as CMake runs them, through the launcher
// CMAKE_C_COMPILER_LAUNCHER names when there is one; and node, which no shim stands in for.
const launcher = process.env["CMAKE_C_COMPILER_LAUNCHER"];
const launched = launcher === undefined ? [] : [launcher];
const cc = process.env["CC"] ?? "";
for (const [program, ...args] of [
    ["cc", "-c", "a.c"],
    ["c++", "-c", "b.cc"],
    ["ar", "rcs", "lib.a", "a.o"],
    [cc, "-c", "c.c"],
    [process.env["CXX"] ?? "", "-c", "f.cc"],
    [path.join(path.dirname(cc), "objcopy"), "a.o"],
    [...launched, cc, "-c", "d.c"],
    [...launched, "cc", "-c", "e.c"],
    ["node", "-e", "0"],
]) {
    spawnSync(program, args, { stdio: "inherit" });
}
