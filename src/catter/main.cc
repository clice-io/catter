#include "js.h"
#include <cassert>
#include <print>
#include <quickjs.h>
#include <string_view>

int main(int argc, char* argv[], char* envp[]) {
    catter::core::js::init_qjs();
    try {
        catter::core::js::run_js_file(
            R"(
        import * as catter from "catter";
        catter.o.print("Hello from Catter!");
    )",
            "inline.js");
    } catch(const catter::qjs::Exception& ex) {
        std::println("JavaScript Exception: {}", ex.what());
    }
    return 0;
}
