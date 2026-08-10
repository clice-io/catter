import { table2table } from "catter/option";
import type { Result } from "catter/neverthrow";

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
 * @returns A `Result` whose `Ok` value is the flattened array containing only
 * non-`clang` `nvcc` argument segments, or whose `Err` value is the parser
 * error string returned while collecting `nvcc` options.
 */
export function nvcc2clang(args: string[]): Result<string[], string> {
  return table2table("clang", "nvcc", args);
}
