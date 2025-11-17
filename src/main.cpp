#include <span>
#include <format>
#include <print>
#include <string>
#include <string_view>

#include "hook/interface.h"

#include "qjs.h"


int main(int argc, char* argv[]) {
    if(argc < 2) {
        std::println("Usage: {} <command>", argv[0]);
        return 1;
    }

    std::span<const char* const> command{argv + 1, argv + argc};

    std::error_code ec;

    auto ret = catter::hook::run(command, ec);

    if(ec) {
        std::println("Failed to attach hook: {}", ec.message());
        return 1;
    }

    if(ret != 0) {
        std::println("Command failed with exit code: {}", ret);
    }

    if(auto captured_output = catter::hook::collect_all(); captured_output.has_value()) {
        for(const auto& line: captured_output.value()) {
            std::println("{}", line);
        }
    } else {
        std::println("Failed to collect captured output: {}", captured_output.error());
        return 1;
    }
    return 0;
}

std::string_view example_script =
    R"(
    import * as catter from "catter";
    catter.add_callback((msg) => {
        catter.log("Callback invoked from JS: " + msg);
    });
)";

void qjs_example() {
    using namespace catter;
    auto rt = qjs::Runtime::create();
    auto ctx = rt.context();

    auto mod = ctx->cmodule("catter");

    auto log = qjs::Function<bool(std::string)>::from(ctx->js_context(), [](std::string msg) {
        std::println("[From JS]: {}", msg);
        return true;
    });
    auto add_callback =
        qjs::Function<void(qjs::Object)>::from(ctx->js_context(), [](qjs::Object cb) {
            std::println("Invoking callback from C++...");
            qjs::Function<void(std::string)>::from(cb)("Hello from C++!");
        });

    mod->add_functor("log", log).add_functor("add_callback", add_callback);

    try {
        ctx->eval(example_script,
                           nullptr,
                           JS_EVAL_TYPE_MODULE | JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_STRICT);
        
    } catch(const catter::qjs::exception& e) {
        std::println("JavaScript Exception: {}", e.what());
    }
}
