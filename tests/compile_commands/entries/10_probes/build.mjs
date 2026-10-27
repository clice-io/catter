// Builds as a configure script does: it compiles a file of its own to see
// what the compiler takes, removes it, and then compiles the project.
import { spawnSync } from "node:child_process";
import { unlinkSync, writeFileSync } from "node:fs";

const msvc = process.argv[2] === "msvc";
const compile = (source, object) =>
    spawnSync(msvc ? "cl" : "cc", msvc ? ["/nologo", "/c", source, `/Fo${object}`] : ["-c", source, "-o", object], {
        stdio: "inherit",
    }).status;

writeFileSync("conftest.c", "int main(void) { return 0; }\n");
compile("conftest.c", "conftest.o");
unlinkSync("conftest.c");
unlinkSync("conftest.o");
process.exitCode = compile("main.c", "main.o") ?? 1;
