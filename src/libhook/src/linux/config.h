#pragma once

#include <string_view>

namespace catter::config {

#if CATTER_LINUX
constexpr static std::string_view RELATIVE_PATH_OF_HOOK_LIB = "libcatter-hook.so";
#elif CATTER_MAC
constexpr static std::string_view RELATIVE_PATH_OF_HOOK_LIB = "libcatter-hook.dylib";
#else
#error "Unsupported platform"
#endif

constexpr static std::string_view HOME_RELATIVE_PATH_OF_LOG = ".cache/catter/hook-log";
}  // namespace catter::config
