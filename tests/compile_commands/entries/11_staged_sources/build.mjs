// Builds as Bazel does: each command runs in a directory of its own, where the
// build stages the command's inputs as symlinks to the files it keeps, and
// which it removes once the command has run, keeping what it wrote; here a
// compile, and a preprocess of the same source. Then the compile once more in
// such a directory the build keeps.
import { spawnSync } from "node:child_process";
import { copyFileSync, mkdirSync, rmdirSync, symlinkSync, unlinkSync } from "node:fs";
import { dirname, join, resolve } from "node:path";

const msvc = process.argv[2] === "msvc";
const inputs = ["src/main.c", "include/greet.h"];

function run(directory, args, written, removed) {
    for (const input of inputs) {
        mkdirSync(join(directory, dirname(input)), { recursive: true });
        symlinkSync(resolve(input), join(directory, input));
    }
    const status = spawnSync(msvc ? "cl" : "cc", args, { cwd: directory, stdio: "inherit" }).status;
    if (removed) {
        copyFileSync(join(directory, written), written);
        unlinkSync(join(directory, written));
        for (const input of inputs) {
            unlinkSync(join(directory, input));
            rmdirSync(join(directory, dirname(input)));
        }
        rmdirSync(directory);
    }
    return status;
}

const compile = msvc
    ? ["/nologo", "/Iinclude", "/c", "src/main.c", "/Fomain.o"]
    : ["-Iinclude", "-c", "src/main.c", "-o", "main.o"];
const preprocess = msvc
    ? ["/nologo", "/Iinclude", "/P", "/Fimain.i", "src/main.c"]
    : ["-Iinclude", "-E", "src/main.c", "-o", "main.i"];
process.exitCode =
    run("sandbox-1", compile, "main.o", true) ||
    run("sandbox-2", preprocess, "main.i", true) ||
    run("kept", compile, "main.o", false) ||
    0;
