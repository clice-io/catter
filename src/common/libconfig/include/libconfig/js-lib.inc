R"(
import * as capi from 'catter-c';

function stdout_print(content) {
    capi.stdout_print(`wrappered by js: ${content}`);
}
let test_cb = null;
function add_test_callback(cb) {
    test_cb = cb;
}
function call_test_callback(msg) {
    if (test_cb) {
        test_cb(`called from js: ${msg}`);
    }
    else {
        stdout_print("no callback registered");
    }
}

export { add_test_callback, call_test_callback, stdout_print };

)"
