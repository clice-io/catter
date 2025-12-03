#include "js.h"
#include "libqjs/qjs.h"
#include <cassert>
#include <quickjs.h>
#include <string_view>

int main(int argc, char* argv[], char* envp[]) {
    catter::core::js::init_qjs();
    catter::core::js::run_js_file(
        R"(
        import * as catter from 'catter';
        catter.add_test_callback((msg) => {
            catter.stdout_print(`Callback received message: ${msg}\n`);
        });
    )",
        "inline.js");
    catter::core::js::run_js_file(
        R"(
        import * as catter from 'catter';
        catter.call_test_callback("Hello from C++!");
    )",
        "inline2.js");
    auto cb_res = catter::core::js::prop_of_js_mod<catter::qjs::Function<void(std::string)>>(
        "call_test_callback");
    assert(cb_res.has_value());
    auto cb = cb_res.value();
    cb("Hello from C++ main!");
    return 0;
}
