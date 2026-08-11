#!/usr/bin/env node
/**
 * Copies the native C API declarations into the raw declaration tree so that
 * output/types/ is self-contained: "catter/native" resolves to
 * output/types/native/index.d.ts for both the package exports and the
 * xpack-installed types directory.
 */
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const outDir = path.join(root, "output", "types", "native");
fs.mkdirSync(outDir, { recursive: true });
fs.copyFileSync(path.join(root, "native", "index.d.ts"), path.join(outDir, "index.d.ts"));
