import terser from "@rollup/plugin-terser";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.dirname(fileURLToPath(import.meta.url));
const modules = JSON.parse(fs.readFileSync(path.join(root, "modules.json"), "utf-8"));
const pkg = JSON.parse(fs.readFileSync(path.join(root, "package.json"), "utf-8"));

const input = {};
const specToFile = {};
for (const [spec, entry] of Object.entries(modules)) {
  const name = spec.slice("catter/".length);
  input[name] = `build/lib/${entry.replace(/^src\//, "").replace(/\.ts$/, ".js")}`;
  specToFile[spec] = `${name}.js`;
}

/**
 * Emits output/lib/manifest.json so the native build can pack every module
 * file into a single embedded blob without maintaining a duplicate list.
 */
function writeManifest() {
  return {
    name: "write-manifest",
    generateBundle(outputOptions, bundle) {
      const files = Object.keys(bundle)
        .filter((fileName) => fileName.endsWith(".js"))
        .map((fileName) => fileName.replace(/^lib\//, ""))
        .sort();
      const manifest = { modules: specToFile, files };
      this.emitFile({
        type: "asset",
        fileName: "lib/manifest.json",
        source: JSON.stringify(manifest, null, 2),
      });
    },
  };
}

/**
 * Assembles the package metadata for the output artifact:
 *  - output/package.json describes every module's ESM entry plus its raw
 *    declaration file (types) derived from modules.json. The native C API
 *    lives at "catter/native" and resolves to output/types/native/index.d.ts.
 */
function writePackageMetadata() {
  const dtsPathFor = (entry) =>
    `types/${entry.replace(/^src\//, "").replace(/\.ts$/, ".d.ts")}`;

  return {
    name: "write-package-metadata",
    generateBundle() {
      const exports = {};
      for (const [spec, entry] of Object.entries(modules)) {
        const mod = spec.slice("catter/".length);
        exports[`./${mod}`] = {
          types: `./${dtsPathFor(entry)}`,
          default: `./lib/${mod}.js`,
        };
      }
      exports["./native"] = { types: "./types/native/index.d.ts" };
      exports["./package.json"] = "./package.json";

      const artifactPkg = {
        name: pkg.name,
        version: pkg.version,
        description: pkg.description,
        license: pkg.license,
        type: "module",
        exports,
        files: ["lib", "types"],
      };
      this.emitFile({
        type: "asset",
        fileName: "package.json",
        source: `${JSON.stringify(artifactPkg, null, 2)}\n`,
      });

    },
  };
}

export default {
  input,
  output: [
    {
      dir: "output",
      format: "es",
      entryFileNames: "lib/[name].js",
      sourcemap: true,
    },
  ],
  plugins: [
    writeManifest(),
    writePackageMetadata(),
    terser({}),
  ],
  external: [/^catter/],
};
