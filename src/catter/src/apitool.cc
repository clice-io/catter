#include "apitool.h"
#include <cstdint>
#include <cstdio>
#include <print>

namespace catter::apitool {
using api_register = void (*)(const catter::qjs::CModule&, const catter::qjs::Context&);

std::vector<api_register>& api_registers() {
    static std::vector<api_register> registers{};
    return registers;
}
}  // namespace catter::apitool

namespace catter::capi::util {
std::filesystem::path absolute_of(std::string js_path) {
    auto js_fs_path = std::filesystem::path(js_path);
    if(js_fs_path.is_absolute()) {
        return js_fs_path;
    }
    // base is js's base
    auto abs_path = (core::js::get_global_runtime_config().pwd / js_fs_path).lexically_normal();
    return abs_path;
};
}  // namespace catter::capi::util
