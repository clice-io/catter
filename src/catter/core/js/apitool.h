#pragma once
#include <filesystem>
#include <format>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include <kota/support/function_traits.h>
#include <kota/async/runtime/task.h>

#include "js.h"
#include "js/qjs.h"
#include "util/log.h"

namespace catter::capi::util {
std::filesystem::path absolute_of(std::string js_path);
}  // namespace catter::capi::util

namespace catter::apitool {
using api_register = void (*)(const catter::qjs::CModule&, const catter::qjs::Context&);

std::vector<api_register>& api_registers();

template <typename T>
std::string serialize_value(const T& value) {
    using U = std::remove_cvref_t<T>;
    if constexpr(std::is_same_v<U, bool> || std::is_integral_v<U>) {
        return std::format("{}", value);
    } else if constexpr(std::is_same_v<U, std::string>) {
        return std::format("\"{}\"", catter::log::escape(value));
    } else if constexpr(std::is_same_v<U, catter::qjs::Parameters>) {
        return std::format("<{} js args>", value.size());
    } else if constexpr(std::is_same_v<U, catter::qjs::Object>) {
        try {
            return qjs::json::stringify(value);
        } catch(const std::exception& e) {
            return std::format("<object stringify failed: {}>", e.what());
        }
    } else {
        static_assert(kota::dependent_false<U>, "Unsupported CAPI value type for logging");
    }
}

template <typename T, typename... Args>
std::string serialize_args(const T& first, const Args&... args) {
    return std::format("[{}]", (serialize_value(first) + ... + (", " + serialize_value(args))));
}

inline std::string serialize_args() {
    return "[]";
}

template <auto Fn>
constexpr std::string_view capi_name() {
    constexpr auto name = kota::meta::pointer_name<{Fn}>();
    return std::string_view{name.data(), name.size()};
}

template <auto V, typename Sign = std::remove_pointer_t<decltype(V)>>
struct hooked {
    static_assert(kota::dependent_false<Sign>, "Unsupported function signature for hooking");
};

template <auto V, typename R, typename... CallArgs>
static R invoke_with_log(const std::string& args_s, CallArgs&&... call_args) {
    try {
        if constexpr(std::is_void_v<R>) {
            V(std::forward<CallArgs>(call_args)...);
            LOG_INFO("Invoke C API `{}`:\n    -> args = {}\n    -> ret = <void>",
                     capi_name<V>(),
                     args_s);
            return;
        } else {
            auto ret = V(std::forward<CallArgs>(call_args)...);
            LOG_INFO("Invoke C API `{}`:\n    -> args = {}\n    -> ret = {}",
                     capi_name<V>(),
                     args_s,
                     serialize_value(ret));
            return ret;
        }
    } catch(const std::exception& e) {
        LOG_INFO("Invoke C API `{}`:\n    -> args = {}\n    -> throw = {}",
                 capi_name<V>(),
                 args_s,
                 e.what());
        throw;
    }
}

template <auto V, typename R, typename... Args>
struct hooked<V, R(Args...)> {
    static R call(Args... args) {
        return invoke_with_log<V, R>(serialize_args(args...), std::forward<Args>(args)...);
    }
};

template <auto V, typename R, typename... Args>
struct hooked<V, R(JSContext*, Args...)> {
    static R call(JSContext* ctx, Args... args) {
        return invoke_with_log<V, R>(serialize_args(args...), ctx, std::forward<Args>(args)...);
    }
};

template <auto FnPtr>
auto to_js_async_function(JSContext* ctx, const char* name) {
    return [&]<typename R, typename... Args>(kota::task<R, std::string> (*)(Args...)) {
        constexpr auto wrapped_fn = +[](JSContext* ctx, Args... args) -> qjs::Promise {
            return qjs::task_to_promise(ctx, js::promise_task_bridge(), FnPtr(std::move(args)...));
        };
        return qjs::Function<qjs::Promise(Args...)>::template from_raw<wrapped_fn>(ctx, name);
    }(FnPtr);
}

template <auto FnPtr>
auto to_js_async_function_with_ctx(JSContext* ctx, const char* name) {
    return [&]<typename R, typename... Args>(kota::task<R, std::string> (*)(JSContext*, Args...)) {
        constexpr auto wrapped_fn = +[](JSContext* ctx, Args... args) -> qjs::Promise {
            return qjs::task_to_promise(ctx,
                                        js::promise_task_bridge(),
                                        FnPtr(ctx, std::move(args)...));
        };
        return qjs::Function<qjs::Promise(Args...)>::template from_raw<wrapped_fn>(ctx, name);
    }(FnPtr);
}

template <auto FnPtr>
auto to_js_function(JSContext* ctx, const char* name) {
    return [&]<typename R, typename... Args>(R (*)(Args...)) {
        return qjs::Function<R(Args...)>::template from_raw<hooked<FnPtr>::call>(ctx, name);
    }(FnPtr);
}

template <auto FnPtr>
auto to_js_function_with_ctx(JSContext* ctx, const char* name) {
    return [&]<typename R, typename... Args>(R (*)(JSContext*, Args...)) {
        return qjs::Function<R(Args...)>::template from_raw<hooked<FnPtr>::call>(ctx, name);
    }(FnPtr);
}
}  // namespace catter::apitool

#define TO_JS_FN(func) catter::apitool::to_js_function<func>(ctx.js_context(), #func)

#define TO_JS_WITHOUT_CTX_FN(func)                                                                 \
    catter::apitool::to_js_function_with_ctx<func>(ctx.js_context(), #func)

#define TO_JS_ASYNC_FN(func) catter::apitool::to_js_async_function<func>(ctx.js_context(), #func)

#define TO_JS_ASYNC_WITHOUT_CTX_FN(func)                                                           \
    catter::apitool::to_js_async_function_with_ctx<func>(ctx.js_context(), #func)

#define MERGE(x, y) x##y
// CAPI(function sign)
#define CAPI(NAME, OTHER)                                                                          \
    auto NAME OTHER;                                                                               \
    static void MERGE(__capi_reg, NAME)(const catter::qjs::CModule& mod,                           \
                                        const catter::qjs::Context& ctx) {                         \
        mod.export_functor(#NAME, TO_JS_FN(NAME));                                                 \
    }                                                                                              \
    static auto MERGE(__capi_reg_instance, NAME) = [] {                                            \
        catter::apitool::api_registers().push_back(MERGE(__capi_reg, NAME));                       \
        return 0;                                                                                  \
    }();                                                                                           \
    auto NAME OTHER

// ASYNC_CAPI(function sign returning kota::task<...>)
#define ASYNC_CAPI(NAME, OTHER)                                                                    \
    auto NAME OTHER;                                                                               \
    static void MERGE(__capi_reg, NAME)(const catter::qjs::CModule& mod,                           \
                                        const catter::qjs::Context& ctx) {                         \
        mod.export_functor(#NAME, TO_JS_ASYNC_FN(NAME));                                           \
    }                                                                                              \
    static auto MERGE(__capi_reg_instance, NAME) = [] {                                            \
        catter::apitool::api_registers().push_back(MERGE(__capi_reg, NAME));                       \
        return 0;                                                                                  \
    }();                                                                                           \
    auto NAME OTHER

// ASYNC_CAPI(function sign returning kota::task<...> without ctx)
#define CTX_ASYNC_CAPI(NAME, OTHER)                                                                \
    auto NAME OTHER;                                                                               \
    static void MERGE(__capi_reg, NAME)(const catter::qjs::CModule& mod,                           \
                                        const catter::qjs::Context& ctx) {                         \
        mod.export_functor(#NAME, TO_JS_ASYNC_WITHOUT_CTX_FN(NAME));                               \
    }                                                                                              \
    static auto MERGE(__capi_reg_instance, NAME) = [] {                                            \
        catter::apitool::api_registers().push_back(MERGE(__capi_reg, NAME));                       \
        return 0;                                                                                  \
    }();                                                                                           \
    auto NAME OTHER

// CAPI(function sign without ctx)
#define CTX_CAPI(NAME, OTHER)                                                                      \
    auto NAME OTHER;                                                                               \
    static void MERGE(__capi_reg, NAME)(const catter::qjs::CModule& mod,                           \
                                        const catter::qjs::Context& ctx) {                         \
        mod.export_functor(#NAME, TO_JS_WITHOUT_CTX_FN(NAME));                                     \
    }                                                                                              \
    static auto MERGE(__capi_reg_instance, NAME) = [] {                                            \
        catter::apitool::api_registers().push_back(MERGE(__capi_reg, NAME));                       \
        return 0;                                                                                  \
    }();                                                                                           \
    auto NAME OTHER
