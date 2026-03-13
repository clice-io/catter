#include "js.h"
#include "eventide/reflection/enum.h"
#include "qjs.h"
#include <cstdio>
#include <cstring>
#include <format>
#include <print>
#include <quickjs.h>
#include "config/js-lib.h"
#include "apitool.h"
#include <optional>

namespace catter::core::js {

namespace {
qjs::Runtime rt;
RuntimeConfig global_config;
qjs::Object js_mod_obj;

std::string error_strace{};
enum class PromiseState { Pending, Fulfilled, Rejected };
PromiseState promise_state = PromiseState::Pending;
}  // namespace

const RuntimeConfig& get_global_runtime_config() {
    return global_config;
}

void sync_eval(std::string_view input, const char* filename, int eval_flags) {
    auto& ctx = rt.context();

    auto promise_obj = ctx.eval(input, filename, eval_flags).as<qjs::Object>();

    if(JS_IsPromise(promise_obj.value())) {
        using Then = qjs::Function<qjs::Object(qjs::Object resolve, qjs::Object reject)>;
        using Catch = qjs::Function<void(qjs::Object error)>;

        promise_state = PromiseState::Pending;
        auto resolve = qjs::Function<void()>::from(ctx.js_context(), []() {
            promise_state = PromiseState::Fulfilled;
        });

        auto reject = qjs::Function<void()>::from(ctx.js_context(),
                                                  []() { promise_state = PromiseState::Rejected; });

        auto error =
            qjs::Function<void(qjs::Object error)>::from(ctx.js_context(), [](qjs::Object error) {
                error_strace = qjs::json::stringify(qjs::Value::from(error));
            });

        promise_obj["then"].as<Then>().invoke(promise_obj,
                                              qjs::Object::from(resolve),
                                              qjs::Object::from(reject));

        promise_obj["catch"].as<Catch>().invoke(promise_obj, qjs::Object::from(error));

        int err;
        JSContext* ctx1;

        while((err = JS_ExecutePendingJob(rt.js_runtime(), &ctx1)) != 0) {
            if(err < 0) {
                throw qjs::Exception("Error while executing pending job.");
                break;
            }
        }
        if(promise_state == PromiseState::Pending) {
            throw qjs::Exception("Inner error after executing js async jobs!");
        }
        if(promise_state == PromiseState::Rejected) {
            throw qjs::Exception(std::format("Module loading with error:\n {}", error_strace));
        }
    }
};

void init_qjs(const RuntimeConfig& config) {
    rt = qjs::Runtime::create();
    global_config = config;

    const qjs::Context& ctx = rt.context();
    auto& mod = ctx.cmodule("catter-c");
    for(auto& reg: catter::apitool::api_registers()) {
        reg(mod, ctx);
    }
    auto js_lib_trim =
        config::data::js_lib.substr(0, config::data::js_lib.find_last_not_of('\0') + 1);
    // init js lib

    sync_eval(js_lib_trim, "catter", JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);
    sync_eval("import * as catter from 'catter'; globalThis.__catter_mod = catter;",
              "get-mod.js",
              JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);

    js_mod_obj = ctx.global_this()["__catter_mod"].as<qjs::Object>();
};

void run_js_file(std::string_view content, const std::string filepath, bool check_error) {
    sync_eval(content, filepath.data(), JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);
}

qjs::Object& js_mod_object() {
    return js_mod_obj;
}

}  // namespace catter::core::js
