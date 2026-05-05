#include "sync.h"

#include <memory>
#include <utility>
#include <quickjs.h>

namespace catter::js {
namespace {
enum class EvalState {
    pending,
    fulfilled,
    rejected,
};

struct EvalResult {
    EvalState state = EvalState::pending;
    std::string error_trace;
};

void wait_for_promise(qjs::Promise promise) {
    auto state = std::make_shared<EvalResult>();
    auto js_ctx = promise.context();

    auto fulfill =
        qjs::Promise::ThenCallback::from(js_ctx, [state]([[maybe_unused]] qjs::Parameters args) {
            state->state = EvalState::fulfilled;
        });
    auto reject = qjs::Promise::ThenCallback::from(js_ctx, [state](qjs::Parameters args) {
        state->state = EvalState::rejected;
        state->error_trace = qjs::format_rejection(args);
    });

    auto then_promise = promise.then(fulfill, reject);
    (void)then_promise;

    while(drain_jobs_with_budget(detail::runtime(), 1024)) {}

    switch(state->state) {
        case EvalState::fulfilled: return;
        case EvalState::rejected: throw qjs::Exception(state->error_trace);
        case EvalState::pending:
            throw qjs::Exception("Promise is still pending after executing all pending jobs.");
    }
}
}  // namespace

void sync_eval(std::string_view input, const char* filename, int eval_flags) {
    auto& ctx = detail::runtime().context();
    auto eval_result = ctx.eval(input, filename, eval_flags);

    if(JS_IsPromise(eval_result.value())) {
        wait_for_promise(qjs::Promise::from_value(std::move(eval_result)));
    }
}

void init_qjs(const RuntimeConfig& config) {
    detail::reset_runtime(config);

    const qjs::Context& ctx = detail::runtime().context();
    detail::register_catter_module(ctx);

    sync_eval(detail::js_lib_source(), "catter", JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);
    sync_eval("import * as catter from 'catter'; globalThis.__catter_mod = catter;",
              "get-mod.js",
              JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);

    js_mod_object() = ctx.global_this()["__catter_mod"].as<qjs::Object>();
}

void run_js_file(std::string_view content, const std::string filepath) {
    sync_eval(content, filepath.data(), JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);
}

}  // namespace catter::js
