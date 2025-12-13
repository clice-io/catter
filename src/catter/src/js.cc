#include "js.h"
#include "libqjs/qjs.h"
#include <cstdio>
#include <cstring>
#include <print>
#include <quickjs.h>
#include "libconfig/js-lib.h"
#include "apitool.h"
#include <optional>
#include <filesystem>

namespace catter::core::js {

namespace {
qjs::Runtime rt;
RuntimeConfig global_config;
qjs::Object js_mod_obj;

char error_strace[512]{};
enum class PromiseState { Pending, Fulfilled, Rejected };
PromiseState promise_state = PromiseState::Pending;
}  // namespace

const RuntimeConfig& get_global_runtime_config() {
    return global_config;
}

void init_qjs(const RuntimeConfig& config) {
    rt = qjs::Runtime::create();
    global_config = config;

    const qjs::Context& ctx = rt.context();
    auto& mod = ctx.cmodule("catter-c");
    for(auto& reg: catter::apitool::api_registers()) {
        reg(mod, ctx);
    }
    // init js lib
    ctx.eval(catter::config::data::js_lib, "catter", JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);
    ctx.eval("import * as catter from 'catter'; globalThis.__catter_mod = catter;",
             "get-mod.js",
             JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);
    try {
        js_mod_obj = ctx.global_this()["__catter_mod"].to<qjs::Object>().value();
    } catch(...) {
        throw qjs::Exception("Failed to get catter module object: " +
                             qjs::detail::dump(ctx.js_context()));
    }
};

static JSValue
    on_promise_resolve(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    // we do not care if successfully resolved.
    promise_state = PromiseState::Fulfilled;
    return JS_UNDEFINED;
}

static JSValue
    on_promise_reject(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    promise_state = PromiseState::Rejected;
    if(argc == 0) {
        std::snprintf(error_strace, sizeof(error_strace), "Promise rejected with no reason");
    }
    JSValue reason = argv[0];
    const char* error_message = JS_ToCString(ctx, reason);

    const char* filename = NULL;
    int line = -1;
    int column = -1;

    // prefer .stack
    if(JS_IsObject(reason)) {
        JSValue stack_val = JS_GetPropertyStr(ctx, reason, "stack");
        if(!JS_IsUndefined(stack_val)) {
            const char* stack_str = JS_ToCString(ctx, stack_val);
            if(stack_str) {
                std::snprintf(error_strace,
                              sizeof(error_strace),
                              "Promise rejected with reason: %s\nStack: %s",
                              error_message ? error_message : "unknown",
                              stack_str);
                JS_FreeCString(ctx, stack_str);
                JS_FreeValue(ctx, stack_val);
                if(error_message)
                    JS_FreeCString(ctx, error_message);
                return JS_UNDEFINED;
            }
        }
        JS_FreeValue(ctx, stack_val);

        // otherwise, use .fileName, .lineNumber, .column
        JSValue filename_val = JS_GetPropertyStr(ctx, reason, "fileName");
        JSValue lineno_val = JS_GetPropertyStr(ctx, reason, "lineNumber");
        JSValue column_val = JS_GetPropertyStr(ctx, reason, "column");

        if(!JS_IsUndefined(filename_val)) {
            filename = JS_ToCString(ctx, filename_val);
        } else {
            filename = "unknown";
        }
        if(!JS_IsUndefined(lineno_val)) {
            JS_ToInt32(ctx, &line, lineno_val);
        }
        if(!JS_IsUndefined(column_val)) {
            JS_ToInt32(ctx, &column, column_val);
        }

        JS_FreeValue(ctx, filename_val);
        JS_FreeValue(ctx, lineno_val);
        JS_FreeValue(ctx, column_val);
    }

    if(filename) {
        std::snprintf(error_strace,
                      sizeof(error_strace),
                      "Promise rejected with reason: %s\nat %s:%d:%d",
                      error_message ? error_message : "unknown",
                      filename,
                      line,
                      column);
    } else {
        std::snprintf(error_strace,
                      sizeof(error_strace),
                      "Promise rejected with reason: %s",
                      error_message ? error_message : "unknown");
    }

    if(error_message)
        JS_FreeCString(ctx, error_message);

    return JS_UNDEFINED;
}

void run_js_file(std::string_view content, const std::string filepath, bool check_error) {
    const qjs::Context& ctx = rt.context();

    auto ret = ctx.eval(content, filepath.data(), JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);
    auto promise_ret = ret.to<qjs::Object>();
    if(!promise_ret.has_value()) {
        throw qjs::Exception("Inner exception!, this exception should not happen.");
    }

    auto promise_obj = promise_ret.value();

    if(JS_IsPromise(promise_obj.value()) && check_error) {
        promise_state = PromiseState::Pending;
        qjs::Value c_resolve_func{
            ctx.js_context(),
            JS_NewCFunction(ctx.js_context(), on_promise_resolve, "__catter_onResolve", 1)};
        qjs::Value c_reject_func{
            ctx.js_context(),
            JS_NewCFunction(ctx.js_context(), on_promise_reject, "__catter_onReject", 1)};

        // promise.then(c_resolve_func, c_reject_func)
        JSValue args[2] = {c_resolve_func.value(), c_reject_func.value()};
        auto then_func = promise_ret.value()["then"];
        JSValue res = JS_Call(ctx.js_context(), then_func.value(), promise_obj.value(), 2, args);

        JSContext* ctx1;
        int err;

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
}

qjs::Object& js_mod_object() {
    return js_mod_obj;
}

}  // namespace catter::core::js
