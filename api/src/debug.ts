/**
 * Debug helpers for assertions inside catter scripts and tests.
 */

import { stdout_print } from "catter-c";

/**
 * Runs a fallback callback when a condition is false.
 *
 * Use this when you want lightweight assertion-style control flow without
 * forcing a throw.
 *
 * @param cond - The boolean condition to validate. When `true`, nothing happens.
 * @param otherwise - The callback to run only when `cond` is `false`.
 *
 * @example
 * ```typescript
 * assertDo(config.options.log, () => {
 *   println("logging is disabled");
 * });
 * ```
 */
export function assertDo(cond: boolean, otherwise: () => void) {
  if (!cond) {
    otherwise();
  }
}

/**
 * Prints an assertion failure message when a condition is false.
 *
 * The message is written to standard output and execution continues.
 *
 * @param cond - The boolean condition that is expected to be `true`.
 *
 * @example
 * ```typescript
 * assertPrint(result.length > 0);
 * ```
 */
export function assertPrint(cond: boolean) {
  assertDo(cond, () => {
    stdout_print("assertion failed!");
  });
}

/**
 * Throws an error when a condition is false.
 *
 * This is the strictest assertion helper in this module and is useful in tests
 * or in scripts that should stop immediately on invalid state.
 *
 * @param cond - The boolean condition that is expected to be `true`.
 *
 * @example
 * ```typescript
 * assertThrow(command.argv.length >= 1);
 * ```
 */
export function assertThrow(cond: boolean) {
  assertDo(cond, () => {
    throw new Error("assertion failed!");
  });
}
