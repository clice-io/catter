#!/usr/bin/env node
/**
 * Validates module import boundaries under api/src.
 *
 * Rules:
 *  - Cross-module dependencies must use the public bare specifier
 *    "catter/<mod>" (or "catter/native" for the native capi), and the
 *    specifier must name a module entry from modules.json, not an internal
 *    file.
 *  - Relative imports ("./x", "../x") are only allowed within the same module.
 *
 * Module membership:
 *  - A directory module owns its whole subtree
 *    (e.g. "catter/cmd" -> src/cmd/).
 *  - A single-file module owns the entry file plus an optional same-stem
 *    companion directory (e.g. "catter/service" -> src/service.ts and
 *    src/service/), so implementation files can still use relative imports
 *    inside the module.
 */
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import ts from "typescript";

const toolRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const pkgRoot = path.resolve(toolRoot, "..");
const srcDir = path.join(pkgRoot, "src");
const modulesJson = JSON.parse(fs.readFileSync(path.join(toolRoot, "modules.json"), "utf-8"));
const moduleSpecs = new Set(Object.keys(modulesJson));

function walkTs(dir, out = []) {
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      walkTs(full, out);
    } else if (entry.name.endsWith(".ts")) {
      out.push(full);
    }
  }
  return out;
}

// Maps every absolute source path to the module spec that owns it.
function buildOwnerMap() {
  const owner = new Map();
  const claim = (spec, file) => {
    const abs = path.resolve(file);
    const existing = owner.get(abs);
    if (existing !== undefined && existing !== spec) {
      throw new Error(
        `[check-modules] ${path.relative(pkgRoot, abs)} is claimed by both "${existing}" and "${spec}"`,
      );
    }
    owner.set(abs, spec);
  };

  for (const [spec, entry] of Object.entries(modulesJson)) {
    const absEntry = path.join(pkgRoot, entry);
    const rel = path.relative(srcDir, absEntry);
    if (rel.includes(path.sep)) {
      // Directory module: the whole subtree belongs to it.
      for (const file of walkTs(path.dirname(absEntry))) {
        claim(spec, file);
      }
    } else {
      // Single-file module: the file itself, plus a same-stem companion dir.
      claim(spec, absEntry);
      const stem = absEntry.replace(/\.ts$/, "");
      if (fs.existsSync(stem) && fs.statSync(stem).isDirectory()) {
        for (const file of walkTs(stem)) {
          claim(spec, file);
        }
      }
    }
  }
  return owner;
}

function resolveRelative(fromFile, spec) {
  const base = path
    .resolve(path.dirname(fromFile), spec)
    .replace(/\.(tsx?|js)$/, "");
  for (const suffix of [".ts", ".tsx", "/index.ts", "/index.tsx"]) {
    const candidate = base + suffix;
    if (fs.existsSync(candidate) && fs.statSync(candidate).isFile()) {
      return candidate;
    }
  }
  return undefined;
}

function checkFile(file, owner, violations) {
  const text = fs.readFileSync(file, "utf8");
  const sourceFile = ts.createSourceFile(file, text, ts.ScriptTarget.Latest, true, ts.ScriptKind.TS);
  const location = (pos) => {
    const { line, character } = sourceFile.getLineAndCharacterOfPosition(pos);
    return `${path.relative(pkgRoot, file)}:${line + 1}:${character + 1}`;
  };

  const check = (spec, pos) => {
    if (spec.startsWith("./") || spec.startsWith("../")) {
      const importerMod = owner.get(path.resolve(file));
      const target = resolveRelative(file, spec);
      const targetMod = target ? owner.get(target) : undefined;
      if (target === undefined) {
        violations.push(`${location(pos)}: unresolved relative import "${spec}"`);
      } else if (importerMod !== targetMod) {
        const hint = targetMod
          ? `use "${targetMod}" instead`
          : "target is not part of any module";
        violations.push(
          `${location(pos)}: cross-module relative import "${spec}" ` +
            `(${importerMod ?? "no module"} -> ${targetMod ?? "no module"}); ${hint}`,
        );
      }
      return;
    }
    if (spec === "catter/native") {
      return;
    }
    if (spec.startsWith("catter/")) {
      if (!moduleSpecs.has(spec)) {
        violations.push(
          `${location(pos)}: non-canonical module specifier "${spec}"; ` +
            "import from a module entry listed in modules.json instead",
        );
      }
    }
  };

  const visit = (node) => {
    if (
      (ts.isImportDeclaration(node) || ts.isExportDeclaration(node)) &&
      node.moduleSpecifier !== undefined &&
      ts.isStringLiteral(node.moduleSpecifier)
    ) {
      check(node.moduleSpecifier.text, node.moduleSpecifier.getStart(sourceFile));
    }
    ts.forEachChild(node, visit);
  };
  visit(sourceFile);
}

function main() {
  const owner = buildOwnerMap();
  const files = walkTs(srcDir);
  const violations = [];
  for (const file of files) {
    checkFile(file, owner, violations);
  }
  if (violations.length > 0) {
    console.error(`[check-modules] ${violations.length} violation(s):`);
    for (const violation of violations.sort()) {
      console.error(`  ${violation}`);
    }
    process.exit(1);
  }
  console.error(`[check-modules] ok: ${files.length} files, ${moduleSpecs.size} modules`);
}

main();
