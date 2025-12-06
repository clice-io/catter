R"(
import * as capi from 'catter-c';
import { stdout_print } from 'catter-c';

/// api for debug and test
/**
 * @param cond - condition to assert
 * @param otherwise - function to call when assertion fails
 */
function assert_do(cond, otherwise) {
    if (!cond) {
        otherwise();
    }
}
/**
 * assert condition, print message on failure
 * @param cond - condition to assert
 */
function assert_print(cond) {
    assert_do(cond, () => {
        stdout_print("assertion failed!");
    });
}
/**
 * assert condition, throw error on failure
 * @param cond - condition to assert
 */
function assert_throw(cond) {
    assert_do(cond, () => {
        throw new Error("assertion failed!");
    });
}

var debug = /*#__PURE__*/Object.freeze({
    __proto__: null,
    assert_do: assert_do,
    assert_print: assert_print,
    assert_throw: assert_throw
});

/**
 * Provides standard output printing
 * @param content - content to print
 */
function print(content) {
    capi.stdout_print(content);
}

var io = /*#__PURE__*/Object.freeze({
    __proto__: null,
    print: print
});

export { debug, io };

)"
