/// api for debug and test

import { stdout_print } from "catter-c";

/**
 * @param cond - condition to assert
 * @param otherwise - function to call when assertion fails
 */
export function assert_do(cond: boolean, otherwise: () => void) {
  if (!cond) {
    otherwise();
  }
}

/**
 * assert condition, print message on failure
 * @param cond - condition to assert
 */
export function assert_print(cond: boolean) {
  assert_do(cond, () => {
    stdout_print("assertion failed!");
  });
}

/**
 * assert condition, throw error on failure
 * @param cond - condition to assert
 */
export function assert_throw(cond: boolean) {
  assert_do(cond, () => {
    throw new Error("assertion failed!");
  });
}
