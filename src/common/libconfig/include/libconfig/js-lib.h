#pragma once

#include <string_view>

extern "C" {
    extern const char _binary_lib_js_start[];
    extern const char _binary_lib_js_end[];
}

namespace catter::config::data {
    const std::string_view js_lib{_binary_lib_js_start, _binary_lib_js_end};
}  // namespace catter::config::data
