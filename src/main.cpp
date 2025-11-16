#include <span>
#include <format>
#include <print>
#include <string>
#include <string_view>

#include "hook/interface.h"
#include "qjs.h"
#include "quickjs.h"

void qjs_example();

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
    }
    return 0;
}

std::string_view example_script =
    R"(
    import * as catter from "catter";

    catter.add_callback((msg) => {
        return `Callback received message: ${msg}`;
    });
    catter._callback((msg) => {
        return `Second callback received message: ${msg}`;
    });
)";

void qjs_example() {
    auto rt = catter::qjs::Runtime::create();
    auto ctx = rt.new_context();

    catter::qjs::Function<std::string(std::string)> stored_func{};

    ctx.new_cmodule("catter")->add_functor(
        "add_callback",
        [&stored_func](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
            -> JSValue {
            if(argc != 1) {
                return JS_ThrowTypeError(ctx, "Expected one argument");
            }
            stored_func = {ctx, argv[0]};
            return JS_UNDEFINED;
        });

    try {
        ctx.eval(example_script,
                 nullptr,
                 JS_EVAL_TYPE_MODULE | JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_STRICT);

        if(stored_func) {
            std::println("Invoking stored callback: {}",
                         stored_func.invoke(ctx.global_this(), "Hello from C++"));
        } else {
            std::println("No callback was stored.");
        }
    } catch(const catter::qjs::exception& e) {
        std::println("JavaScript Exception: {}", e.what());
    }
}
