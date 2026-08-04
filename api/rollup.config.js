import typescript from "@rollup/plugin-typescript";
import resolve from "@rollup/plugin-node-resolve";
import terser from "@rollup/plugin-terser";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.dirname(fileURLToPath(import.meta.url));
const modules = JSON.parse(fs.readFileSync(path.join(root, "modules.json"), "utf-8"));

const input = {};
const specToFile = {};
for (const [spec, entry] of Object.entries(modules)) {
  const name = spec === "catter" ? "catter" : spec.slice("catter/".length);
  input[name] = entry;
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
      dir: "output/lib",
      format: "es",
      entryFileNames: "[name].js",
    },
  ],
  plugins: [
    resolve({
      extensions: [".ts", ".js"],
      browser: false,
    }),
    typescript({
      tsconfig: "./tsconfig.rollup.json",
      compilerOptions: {
        declarationDir: undefined,

        declaration: false,
        declarationMap: false,

        module: "esnext",
        moduleResolution: "bundler",
      },
    }),
    writeManifest(),
    terser({}),
  ],
  external: [/^catter/],
};
