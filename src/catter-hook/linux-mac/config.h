#pragma once

namespace catter::config {
constexpr static char OS_PATH_SEPARATOR = ':';
constexpr static char OS_DIR_SEPARATOR = '/';

}  // namespace catter::config

namespace catter::config::hook {
constexpr static char KEY_CATTER_PROXY_PATH[] = "__key_catter_proxy_path_v1";
constexpr static char KEY_CATTER_COMMAND_ID[] = "__key_catter_command_id_v1";
constexpr static char ERROR_PREFIX[] = "linux or mac error found in hook:";

#if defined(CATTER_LINUX)
constexpr static char KEY_PRELOAD[] = "LD_PRELOAD";
constexpr static char LD_PRELOAD_INIT_ENTRY[] = "LD_PRELOAD=";
#elif defined(CATTER_MAC)
constexpr static char KEY_PRELOAD[] = "DYLD_INSERT_LIBRARIES";
constexpr static char LD_PRELOAD_INIT_ENTRY[] = "DYLD_INSERT_LIBRARIES=";
#endif

#ifdef CATTER_LINUX
constexpr static char RELATIVE_PATH_OF_HOOK_LIB[] = "libcatter-hook-unix.so";
#elif defined(CATTER_MAC)
constexpr static char RELATIVE_PATH_OF_HOOK_LIB[] = "libcatter-hook-unix.dylib";
#endif

constexpr static char LOG_PATH_REL[] = "log/catter-hook.log";

}  // namespace catter::config::hook

namespace catter::config::proxy {
constexpr static char CATTER_PROXY_ENV_KEY[] = "exec_is_catter_proxy_v1";
}  // namespace catter::config::proxy
