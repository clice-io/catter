#pragma once

#include <array>
#include <string_view>

#include "support/config/session.h"

namespace catter::config {
constexpr static char OS_PATH_SEPARATOR = ':';
constexpr static char OS_DIR_SEPARATOR = '/';

constexpr static char KEY_CATTER_PROXY_PATH[] = "__key_catter_proxy_path_v1";
constexpr static auto KEYS_TO_INJECT =
    std::array<std::string_view, 3>{KEY_CATTER_PROXY_PATH, KEY_CATTER_COMMAND_ID, KEY_CATTER_IPC};

#if defined(CATTER_LINUX)
constexpr static char KEY_PRELOAD[] = "LD_PRELOAD";
/// What the loader splits KEY_PRELOAD at, with no way to escape them: a path
/// that holds one cannot be preloaded.
constexpr static char PRELOAD_SEPARATORS[] = ": ";
constexpr static char PRELOAD_SEPARATORS_NAMED[] = "spaces and colons";
constexpr static char LD_PRELOAD_INIT_ENTRY[] = "LD_PRELOAD=";
constexpr static char RELATIVE_PATH_OF_HOOK_LIB[] = "libcatter-hook-unix.so";
constexpr static char HOOK_LIB_NAME[] = "libcatter-hook-unix.so";
#elif defined(CATTER_MAC)
constexpr static char KEY_PRELOAD[] = "DYLD_INSERT_LIBRARIES";
constexpr static char PRELOAD_SEPARATORS[] = ":";
constexpr static char PRELOAD_SEPARATORS_NAMED[] = "colons";
constexpr static char LD_PRELOAD_INIT_ENTRY[] = "DYLD_INSERT_LIBRARIES=";
constexpr static char RELATIVE_PATH_OF_HOOK_LIB[] = "libcatter-hook-unix.dylib";
constexpr static char HOOK_LIB_NAME[] = "libcatter-hook-unix.dylib";
#endif

constexpr static char HOOK_LOG_PATH[] = "log/catter-hook.log";

}  // namespace catter::config
