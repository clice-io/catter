#pragma once

#include <string_view>

namespace catter::config {
constexpr static std::string_view RELATIVE_PATH_OF_HOOK_LIB = "libcatter-hook-linux.so";
constexpr static std::string_view HOME_RELATIVE_PATH_OF_LOG = ".cache/catter/hook-log";
}  // namespace catter::config
