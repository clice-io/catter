#!/usr/bin/env node
/**
 * Generates the public type declarations for every builtin module.
 *
 * 1. Runs `tsc` to emit the per-file declaration tree under `build/types/`.
 * 2. Rolls each `modules.json` entry into a flat per-module declaration at
 *    `output/types/catter/<mod>.d.ts`, so `catter/<mod>` subpath imports
 *    resolve to exactly their own module's declarations.
 */
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { Extractor, ExtractorConfig } from "@microsoft/api-extractor";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const modules = JSON.parse(fs.readFileSync(path.join(root, "modules.json"), "utf-8"));
const configFile = path.join(root, "api-extractor.json");
const baseConfig = ExtractorConfig.loadFile(configFile);

function dtsPathFor(entry) {
  return `build/types/${entry.replace(/^src\//, "").replace(/\.ts$/, ".d.ts")}`;
}

function runExtractor(label, entryDts, outputDts) {
  const extractorConfig = ExtractorConfig.prepare({
    configObject: {
      ...baseConfig,
      mainEntryPointFilePath: `<projectFolder>/${entryDts}`,
      dtsRollup: {
        ...baseConfig.dtsRollup,
        untrimmedFilePath: `<projectFolder>/${outputDts}`,
      },
    },
    configObjectFullPath: configFile,
    packageJsonFullPath: path.join(root, "package.json"),
    projectFolderLookupToken: root,
  });
  console.error(`[build-types] extracting ${label} -> ${outputDts}`);
  const result = Extractor.invoke(extractorConfig, { localBuild: false });
  if (!result.succeeded) {
    throw new Error(`API Extractor failed for '${label}'`);
  }
}

// Per-module entries.
for (const [spec, entry] of Object.entries(modules)) {
  const mod = spec.slice("catter/".length);
  runExtractor(spec, dtsPathFor(entry), `output/types/${mod}.d.ts`);
}
