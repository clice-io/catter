/// api for debug and test

import { stdout_print } from "catter-c";

/**
 * @param cond - condition to assert
 * @param otherwise - function to call when assertion fails
 */
export function assertDo(cond: boolean, otherwise: () => void) {
  if (!cond) {
    otherwise();
  }
}

/**
 * assert condition, print message on failure
 * @param cond - condition to assert
 */
export function assertPrint(cond: boolean) {
  assertDo(cond, () => {
    stdout_print("assertion failed!");
  });
}

/**
 * assert condition, throw error on failure
 * @param cond - condition to assert
 */
export function assertThrow(cond: boolean) {
  assertDo(cond, () => {
    throw new Error("assertion failed!");
  });
}
