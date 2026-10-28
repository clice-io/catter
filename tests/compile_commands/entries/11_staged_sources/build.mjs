// Compiles as Bazel does: in a directory of its own, where the build stages
// the compile's inputs as symlinks to the files it keeps, and which it removes
// once the compile has run, keeping the object; then once more in such a
// directory the build keeps.
import { spawnSync } from "node:child_process";
import { copyFileSync, mkdirSync, rmdirSync, symlinkSync, unlinkSync } from "node:fs";
import { dirname, join, resolve } from "node:path";

const msvc = process.argv[2] === "msvc";
const inputs = ["src/main.c", "include/greet.h"];

function compile(directory) {
    for (const input of inputs) {
        mkdirSync(join(directory, dirname(input)), { recursive: true });
        symlinkSync(resolve(input), join(directory, input));
    }
    const args = msvc
        ? ["/nologo", "/Iinclude", "/c", "src/main.c", "/Fomain.o"]
        : ["-Iinclude", "-c", "src/main.c", "-o", "main.o"];
    return spawnSync(msvc ? "cl" : "cc", args, { cwd: directory, stdio: "inherit" }).status;
}

const status = compile("sandbox");
copyFileSync(join("sandbox", "main.o"), "main.o");
for (const input of inputs) {
    unlinkSync(join("sandbox", input));
    rmdirSync(join("sandbox", dirname(input)));
}
unlinkSync(join("sandbox", "main.o"));
rmdirSync("sandbox");
process.exitCode = status || compile("kept") || 0;
