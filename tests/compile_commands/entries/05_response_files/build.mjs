// Compiles as MSBuild does: the arguments go to the compiler in a response
// file, which the build writes before the compiler runs and deletes once it
// has.
import { spawnSync } from "node:child_process";
import { rmSync, writeFileSync } from "node:fs";

if (process.argv[2] === "msvc") {
    const text = String.raw`/nologo /EHsc /DCATTER_TEST=1 "/DGREETING=\"hello world\"" /Iinclude src/main.cc src/util.cc /Fehello.exe`;
    // MSBuild writes its response files in UTF-16, after a byte order mark.
    writeFileSync("args.rsp", "\ufeff" + text, "utf16le");
} else {
    const text = String.raw`-DCATTER_TEST=1 "-DGREETING=\"hello world\"" -Iinclude src/main.cc src/util.cc -o hello`;
    writeFileSync("args.rsp", text);
}
const child = spawnSync(process.argv[2] === "msvc" ? "cl" : "c++", ["@args.rsp"], { stdio: "inherit" });
rmSync("args.rsp");
process.exitCode = child.status ?? 1;
