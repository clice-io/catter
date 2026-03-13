#pragma once
#include <filesystem>
#include <format>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <eventide/common/function_traits.h>

#include "util/log.h"

#include "qjs.h"

namespace catter::apitool {
using api_register = void (*)(const catter::qjs::CModule&, const catter::qjs::Context&);

std::vector<api_register>& api_registers();

template <typename Tuple, typename Ret>
struct remove_first_param_signature {
    static_assert(eventide::dependent_false<Tuple>, "Function must have at least one parameter");
};

template <typename First, typename... Args, typename Ret>
struct remove_first_param_signature<std::tuple<First, Args...>, Ret> {
    using type = Ret(Args...);
};

template <typename Fn>
using without_first_param_t =
    typename remove_first_param_signature<eventide::function_args_t<Fn>,
                                          eventide::function_return_t<Fn>>::type;

template <typename T>
std::string serialize_value(const T& value) {
    using U = std::remove_cvref_t<T>;
    if constexpr(std::is_same_v<U, bool>) {
        return value ? "true" : "false";
    } else if constexpr(std::is_same_v<U, std::string>) {
        return std::format("\"{}\"", value);
    } else if constexpr(std::is_integral_v<U>) {
        if constexpr(std::is_signed_v<U>) {
            return std::to_string(static_cast<int64_t>(value));
        } else {
            return std::to_string(static_cast<uint64_t>(value));
        }
    } else if constexpr(std::is_same_v<U, catter::qjs::Object>) {
        try {
            return catter::qjs::Value::from(value).stringify();
        } catch(const std::exception& e) {
            return std::format("<object stringify failed: {}>", e.what());
        }
    } else {
        static_assert(eventide::dependent_false<U>, "Unsupported CAPI value type for logging");
    }
}

template <typename... Args>
std::string serialize_args(const Args&... args) {
    std::string result{"["};
    bool first = true;
    auto append_one = [&](const auto& arg) {
        if(!first) {
            result += ", ";
        }
        result += serialize_value(arg);
        first = false;
    };
    (append_one(args), ...);
    result += "]";
    return result;
}

inline std::string serialize_js_value(JSContext* ctx, JSValueConst value) {
    try {
        return catter::qjs::Value{ctx, value}.stringify();
    } catch(const std::exception& e) {
        return std::format("<jsvalue stringify failed: {}>", e.what());
    }
}

inline std::string serialize_bare_args(JSContext* ctx, int argc, JSValueConst* argv) {
    std::string result{"["};
    for(int i = 0; i < argc; ++i) {
        if(i != 0) {
            result += ", ";
        }
        result += serialize_js_value(ctx, argv[i]);
    }
    result += "]";
    return result;
}

template <auto Fn>
struct hooked;

template <auto Fn>
constexpr std::string_view capi_name() {
    constexpr auto name = eventide::refl::pointer_name<{Fn}>();
    return std::string_view{name.data(), name.size()};
}

template <typename R, typename... Args, R (*Fn)(Args...)>
struct hooked<Fn> {
    static R call(Args... args) {
        const auto args_s = serialize_args(args...);
        try {
            if constexpr(std::is_void_v<R>) {
                Fn(std::forward<Args>(args)...);
                LOG_INFO("capi {} args={} ret=<void>", capi_name<Fn>(), args_s);
                return;
            } else {
                auto ret = Fn(std::forward<Args>(args)...);
                LOG_INFO("capi {} args={} ret={}", capi_name<Fn>(), args_s, serialize_value(ret));
                return ret;
            }
        } catch(const std::exception& e) {
            LOG_INFO("capi {} args={} throw={}", capi_name<Fn>(), args_s, e.what());
            throw;
        }
    }
};

template <typename R, typename... Args, R (*Fn)(JSContext*, Args...)>
struct hooked<Fn> {
    static R call(JSContext* ctx, Args... args) {
        const auto args_s = serialize_args(args...);
        try {
            if constexpr(std::is_void_v<R>) {
                Fn(ctx, std::forward<Args>(args)...);
                LOG_INFO("capi {} args={} ret=<void>", capi_name<Fn>(), args_s);
                return;
            } else {
                auto ret = Fn(ctx, std::forward<Args>(args)...);
                LOG_INFO("capi {} args={} ret={}", capi_name<Fn>(), args_s, serialize_value(ret));
                return ret;
            }
        } catch(const std::exception& e) {
            LOG_INFO("capi {} args={} throw={}", capi_name<Fn>(), args_s, e.what());
            throw;
        }
    }
};

template <JSValue (*Fn)(JSContext*, JSValueConst, int, JSValueConst*)>
struct hooked<Fn> {
    static JSValue call(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
        const auto args_s = serialize_bare_args(ctx, argc, argv);
        try {
            auto ret = Fn(ctx, this_val, argc, argv);
            LOG_INFO("capi {} args={} ret={}",
                     capi_name<Fn>(),
                     args_s,
                     serialize_js_value(ctx, ret));
            return ret;
        } catch(const std::exception& e) {
            LOG_INFO("capi {} args={} throw={}", capi_name<Fn>(), args_s, e.what());
            throw;
        }
    }
};
}  // namespace catter::apitool

#define TO_JS_FN(func)                                                                             \
    catter::qjs::Function<decltype(func)>::from_raw<catter::apitool::hooked<func>::call>(          \
        ctx.js_context(),                                                                          \
        #func)

#define TO_JS_WITHOUT_CTX_FN(func)                                                                 \
    catter::qjs::Function<catter::apitool::without_first_param_t<decltype(func)>>::from_raw<       \
        catter::apitool::hooked<func>::call>(ctx.js_context(), #func)

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

// CAPI(function sign)
#define BARE_CAPI(ARGC, NAME)                                                                      \
    auto NAME(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue;     \
    static void MERGE(__capi_reg, NAME)(const catter::qjs::CModule& mod,                           \
                                        const catter::qjs::Context& ctx) {                         \
        mod.export_bare_functor(#NAME, catter::apitool::hooked<NAME>::call, ARGC);                 \
    }                                                                                              \
    static auto MERGE(__capi_reg_instance, NAME) = [] {                                            \
        catter::apitool::api_registers().push_back(MERGE(__capi_reg, NAME));                       \
        return 0;                                                                                  \
    }();                                                                                           \
    auto NAME(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue

namespace catter::capi::util {
std::filesystem::path absolute_of(std::string js_path);
}  // namespace catter::capi::util
