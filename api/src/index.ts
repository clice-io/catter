import * as capi from "catter-c";
export function stdout_print(content: string) {
  capi.stdout_print(`wrappered by js: ${content}`);
}

export type test_callback = (msg: string) => void;

let test_cb: test_callback | null = null;

export function add_test_callback(cb: test_callback) {
  test_cb = cb;
}

export function call_test_callback(msg: string) {
  if (test_cb) {
    test_cb(`called from js: ${msg}`);
  } else {
    stdout_print("no callback registered");
  }
}
