// Regenerates the option identifier enums of api/src/option/*.ts from the
// TableGen option tables of the llvm-option-inc package, in the order of their
// OPTION(...) rows, which is the order of the IDs in src/common/option.
//
//   node scripts/js-gen/generate_llvm_option.mjs [<dir with llvm-options-td>]
//
// The directory defaults to $CONDA_PREFIX/include (pixi run).

import fs from "node:fs";
import path from "node:path";

const root = path.resolve(import.meta.dirname, "..", "..");
const include = process.argv[2] ?? path.join(process.env.CONDA_PREFIX ?? "", "include");

const tables = {
  clang: ["clang-Driver-Options.inc", "ClangID"],
  "lld-coff": ["lld-COFF-Options.inc", "LldCoffID"],
  "lld-elf": ["lld-ELF-Options.inc", "LldElfID"],
  "lld-macho": ["lld-MachO-Options.inc", "LldMachOID"],
  "lld-mingw": ["lld-MinGW-Options.inc", "LldMinGWID"],
  "lld-wasm": ["lld-wasm-Options.inc", "LldWasmID"],
  "llvm-dlltool": ["llvm-dlltool-Options.inc", "LlvmDlltoolID"],
  "llvm-lib": ["llvm-lib-Options.inc", "LlvmLibID"],
};

// Every file is checked before any is written.
const updates = [];
for (const [name, [inc, enumName]] of Object.entries(tables)) {
  const source = fs.readFileSync(path.join(include, "llvm-options-td", inc), "utf8");
  // The ID is the third field of an OPTION( line; comments name the option
  // and may hold any character.
  const ids = source
    .split(/\r?\n/)
    .filter((line) => line.startsWith("OPTION("))
    .map((line) => line.slice("OPTION(".length).replace(/\/\*.*?\*\//g, "").split(",")[2].trim());
  const file = path.join(root, "api", "src", "option", `${name}.ts`);
  const text = fs.readFileSync(file, "utf8");
  const eol = text.includes("\r\n") ? "\r\n" : "\n";
  const body = ["  ID_INVALID = 0,", ...ids.map((id) => `  ID_${id},`)].join(eol);
  const pattern = new RegExp(`(export enum ${enumName} \\{\\r?\\n)[\\s\\S]*?(\\r?\\n\\})`);
  if (!pattern.test(text)) throw new Error(`${file}: no enum ${enumName}`);
  updates.push({ file, text: text.replace(pattern, (_, open, close) => open + body + close), count: ids.length });
}
for (const { file, text, count } of updates) {
  fs.writeFileSync(file, text);
  console.log(`${file}: ${count} options`);
}
