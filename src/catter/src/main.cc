#include "libqjs.h"

#include <print>
using namespace catter;

std::string_view example_script =
    R"(
    import * as catter from "catter";

    try {
        catter.add_callback(() => {
            // Invalid callback: missing parameter
        });
    } catch (e) {
        catter.log("Caught exception when adding invalid callback: " + e.message);
    }

    catter.add_callback((msg) => {
        catter.log("Callback invoked from JS: " + msg);
        throw new Error("Test exception from JS callback");
    });

)";

struct Handler {
    auto callback_setter(const qjs::Context& ctx) {
        return qjs::Function<void(qjs::Object)>::from(
            ctx.js_context(),
            [this](qjs::Object callback) {
                auto cb = callback.to<qjs::Function<void(std::string)>>();
                if(cb.has_value()) {
                    this->callback = cb.value();
                } else {
                    throw qjs::Exception("Invalid callback function provided");
                }
            });
    }

    qjs::Function<void(std::string)> callback{};
};

void qjs_example() {
    auto rt = qjs::Runtime::create();

    const qjs::Context& ctx = rt.context();

    auto log = qjs::Function<bool(std::string)>::from(ctx.js_context(), [](std::string msg) {
        std::println("[From JS]: {}", msg);
        return true;
    });

    Handler handler{};
    ctx.cmodule("catter")
        .export_functor("log", log)
        .export_functor("add_callback", handler.callback_setter(ctx));

    try {
        ctx.eval(example_script,
                 nullptr,
                 JS_EVAL_TYPE_MODULE | JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_STRICT);
        if(handler.callback) {
            handler.callback.invoke(ctx.global_this(), "Hello from C++!");
        } else {
            std::println("No callback was set from JS");
        }

    } catch(const catter::qjs::Exception& e) {
        std::println("JavaScript Exception: {}", e.what());
    }
}

int main(int argc, char* argv[], char* envp[]) {
    qjs_example();
    return 0;
}
