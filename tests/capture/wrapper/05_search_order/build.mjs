import { spawnSync } from "node:child_process";
import fs from "node:fs";
import path from "node:path";

// Looks for a C++ compiler as CMake does for Ninja on Windows: c++ or g++, in that order, from
// the first directory on PATH that has either; then runs c++ by its name.
function find(names) {
    for (const directory of (process.env["PATH"] ?? "").split(path.delimiter)) {
        for (const name of names) {
            const program = path.join(directory, name);
            try {
                fs.accessSync(program, fs.constants.X_OK);
                return program;
            } catch {
                continue;
            }
        }
    }
    return names[0];
}

for (const [program, ...args] of [
    [find(["c++", "g++"]), "-c", "a.cc"],
    ["c++", "-c", "b.cc"],
]) {
    spawnSync(program, args, { stdio: "inherit" });
}
