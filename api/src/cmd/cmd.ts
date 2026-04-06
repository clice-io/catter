import type { Compiler } from "catter-c";

import * as option from "../option/index.js";

export { identify_compiler, Compiler } from "catter-c";

/**
 * Filters an `nvcc` argument list down to the segments that are not valid
 * `clang` options.
 *
 * The input is first parsed with the `nvcc` option table and then split back
 * into per-option argument spans. Any span that can also be parsed as a
 * `clang` option is discarded, and the remaining spans are flattened into the
 * returned argument array.
 *
 * @param args - Raw command-line arguments to inspect, usually without the
 * executable name.
 * @returns A flattened array containing only non-`clang` `nvcc` argument
 * segments, or the parser error string returned while collecting `nvcc`
 * options.
 */
export function nvcc2clang(args: string[]): string | string[] {
  return option.table2table("clang", "nvcc", args);
}

export const CompilerKind = [
  "gcc",
  "clang",
  "flang",
  "ifort",
  "crayftn",
  "nvcc",
  "wrapper",
  "unknown",
] as const;

const _CompilerKindCheck: (typeof CompilerKind)[number] = {} as Compiler;
