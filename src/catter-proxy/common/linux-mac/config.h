#pragma once

namespace catter::config {
constexpr const inline static char OS_PATH_SEPARATOR = ':';
constexpr const inline static char OS_DIR_SEPARATOR = '/';

}  // namespace catter::config

namespace catter::config::hook {
constexpr const inline static char KEY_CATTER_PROXY_PATH[] = "__key_catter_proxy_path_v1";
constexpr const inline static char KEY_CATTER_COMMAND_ID[] = "__key_catter_command_id_v1";
constexpr const inline static char ERROR_PREFIX[] = "linux or mac error found in hook:";

#if defined(CATTER_LINUX)
constexpr const inline static char KEY_PRELOAD[] = "LD_PRELOAD";
constexpr const inline static char LD_PRELOAD_INIT_ENTRY[] = "LD_PRELOAD=";
#elif defined(CATTER_MAC)
constexpr const inline static char KEY_PRELOAD[] = "DYLD_INSERT_LIBRARIES";
constexpr const inline static char LD_PRELOAD_INIT_ENTRY[] = "DYLD_INSERT_LIBRARIES=";
#else
#error "Unsupported platform"
#endif

#ifdef CATTER_LINUX
constexpr static const char* RELATIVE_PATH_OF_HOOK_LIB = "libcatter-hook-unix.so";
#elif defined(CATTER_MAC)
constexpr static const char* RELATIVE_PATH_OF_HOOK_LIB = "libcatter-hook-unix.dylib";
#else
#error "Unsupported platform"
#endif

constexpr const inline static char LOG_PATH_REL[] = "log/catter-hook.log";

}  // namespace catter::config::hook

namespace catter::config::proxy {
constexpr const inline static char CATTER_PROXY_ENV_KEY[] = "exec_is_catter_proxy_v1";
}  // namespace catter::config::proxy
