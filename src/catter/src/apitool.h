#pragma once
#include "libqjs/qjs.h"

namespace catter::apitool {
using api_register = void (*)(const catter::qjs::CModule&, const catter::qjs::Context&);
inline std::vector<api_register> api_registers{};
}  // namespace catter::apitool

#define TO_JS_FN(func)                                                                             \
    catter::qjs::Function<decltype(func)>::from_raw<func>(ctx.js_context(), #func)

#define MERGE(x, y) x##y
// CAPI(function sign)
#define CAPI(NAME, OTHER)                                                                          \
    auto NAME OTHER;                                                                               \
    static void MERGE(__capi_reg, name)(const catter::qjs::CModule& mod,                           \
                                        const catter::qjs::Context& ctx) {                         \
        mod.export_functor(#NAME, TO_JS_FN(NAME));                                                 \
    }                                                                                              \
    static auto MERGE(__capi_reg_instance, __COUNTER__) = [] {                                     \
        catter::apitool::api_registers.push_back(MERGE(__capi_reg, name));                         \
        return 0;                                                                                  \
    }();                                                                                           \
    auto NAME OTHER
