#!/usr/bin/env node
/**
 * Generates the public type declarations for every builtin module.
 *
 * 1. Runs `tsc` to emit the per-file declaration tree under `build/types/`.
 * 2. Copies the aggregate entry to `output/types/index.d.ts` verbatim, so the
 *    aggregate re-exports the same `catter/<mod>` declarations as subpath
 *    consumers (preserving type identity for classes with private members).
 * 3. Rolls each `modules.json` entry into a flat per-module declaration at
 *    `output/types/catter/<mod>.d.ts`, so `catter/<mod>` subpath imports
 *    resolve to exactly their own module's declarations.
 */
import { execFileSync } from "node:child_process";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { Extractor, ExtractorConfig } from "@microsoft/api-extractor";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const modules = JSON.parse(fs.readFileSync(path.join(root, "modules.json"), "utf-8"));
const tscBin = path.join(root, "node_modules", "typescript", "bin", "tsc");

function dtsPathFor(entry) {
  return `build/types/${entry.replace(/^src\//, "").replace(/\.ts$/, ".d.ts")}`;
}

function runExtractor(label, entryDts, outputDts) {
  const configObject = {
    projectFolder: root,
    mainEntryPointFilePath: `<projectFolder>/${entryDts}`,
    compiler: {
      tsconfigFilePath: "<projectFolder>/tsconfig.types.json",
    },
    apiReport: {
      enabled: false,
    },
    docModel: {
      enabled: false,
    },
    dtsRollup: {
      enabled: true,
      untrimmedFilePath: `<projectFolder>/${outputDts}`,
    },
    messages: {
      extractorMessageReporting: {
        "ae-missing-release-tag": { logLevel: "none" },
      },
    },
    tsdocMetadata: {
      enabled: false,
    },
  };

  const extractorConfig = ExtractorConfig.prepare({
    configObject,
    configObjectFullPath: path.join(root, "api-extractor.json"),
    packageJsonFullPath: path.join(root, "package.json"),
    projectFolderLookupToken: root,
  });
  console.error(`[build-types] extracting ${label} -> ${outputDts}`);
  const result = Extractor.invoke(extractorConfig, { localBuild: false });
  if (!result.succeeded) {
    throw new Error(`API Extractor failed for '${label}'`);
  }
}

// 1. Per-file declarations.
execFileSync(process.execPath, [tscBin, "--project", "tsconfig.app.json"], {
  cwd: root,
  stdio: "inherit",
});

// 2. Aggregate entry. Copy the tsc-emitted declarations verbatim so the
//    aggregate keeps re-exporting the very same `catter/<mod>` modules that
//    subpath consumers resolve to. Bundling the namespaces into a separate
//    declaration would break type identity for classes with private members.
fs.mkdirSync(path.join(root, "output", "types"), { recursive: true });
fs.copyFileSync(
  path.join(root, dtsPathFor(modules.catter)),
  path.join(root, "output", "types", "index.d.ts"),
);

// 3. Per-module entries.
for (const [spec, entry] of Object.entries(modules)) {
  if (spec === "catter") {
    continue;
  }
  const mod = spec.slice("catter/".length);
  runExtractor(spec, dtsPathFor(entry), `output/types/catter/${mod}.d.ts`);
}
