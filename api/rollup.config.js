import typescript from "@rollup/plugin-typescript";
import resolve from "@rollup/plugin-node-resolve";

export default {
  input: "src/index.ts",
  output: [
    {
      file: "output/lib/lib.js",
      format: "es",
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
        moduleResolution: "bundler"
      },
    }),
  ],
  external: ['catter-c']
};
