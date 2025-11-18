
#include "qjs.h"
#include <print>
using namespace catter;

std::string_view example_script =
    R"(
    import * as catter from "catter";
    catter.add_callback((msg) => {
        catter.log("Callback invoked from JS: " + msg);
    });
)";

struct Handler {
    auto callback_setter(const qjs::Context& ctx) {
        return qjs::Function<void(qjs::Object)>::from(
            ctx.js_context(),
            [this](qjs::Object callback) {
                this->callback = qjs::Function<void(std::string)>::from(callback);
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

        handler.callback.invoke(ctx.global_this(), "Hello from C++!");
    } catch(const catter::qjs::exception& e) {
        std::println("JavaScript Exception: {}", e.what());
    }
}
