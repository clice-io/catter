/// api for debug and test

import { stdout_print } from "catter-c";

/**
 * Runs a fallback callback when a condition is false.
 *
 * @param cond - Condition to check.
 * @param otherwise - Callback invoked when `cond` is `false`.
 */
export function assertDo(cond: boolean, otherwise: () => void) {
  if (!cond) {
    otherwise();
  }
}

/**
 * Prints an assertion failure message when a condition is false.
 *
 * @param cond - Condition to assert.
 */
export function assertPrint(cond: boolean) {
  assertDo(cond, () => {
    stdout_print("assertion failed!");
  });
}

/**
 * Throws an error when a condition is false.
 *
 * @param cond - Condition to assert.
 */
export function assertThrow(cond: boolean) {
  assertDo(cond, () => {
    throw new Error("assertion failed!");
  });
}
