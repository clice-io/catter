#pragma once

namespace catter::config::proxy {
constexpr static char LOG_PATH_REL[] = "log/catter-proxy.log";
#ifdef CATTER_WINDOWS
constexpr static char EXE_NAME[] = "catter-proxy.exe";
#else
constexpr static char EXE_NAME[] = "catter-proxy";
#endif
};  // namespace catter::config::proxy
