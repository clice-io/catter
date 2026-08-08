import terser from "@rollup/plugin-terser";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.dirname(fileURLToPath(import.meta.url));
const manifest = JSON.parse(fs.readFileSync(path.join(root, "manifest.json"), "utf-8"));

const input = {};
const specToFile = {};
for (const [spec, entry] of Object.entries(manifest)) {
  const name = spec.replace(/^script::/, "");
  input[name] = `build/${entry.replace(/^src\//, "").replace(/\.ts$/, ".js")}`;
  specToFile[spec] = `${name}.js`;
}

/**
 * Emits output/lib/manifest.json so the native build can pack every script
 * file into a single embedded blob without maintaining a duplicate list.
 */
function writeManifest() {
  return {
    name: "write-manifest",
    generateBundle(outputOptions, bundle) {
      const files = Object.keys(bundle)
        .filter((fileName) => fileName.endsWith(".js"))
        .sort();
      const manifest = { modules: specToFile, files };
      this.emitFile({
        type: "asset",
        fileName: "manifest.json",
        source: JSON.stringify(manifest, null, 2),
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
      entryFileNames: "[name].js",
    },
  ],
  plugins: [
    writeManifest(),
    terser({}),
  ],
  external: [/^catter/],
};
