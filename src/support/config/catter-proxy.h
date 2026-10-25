#pragma once

namespace catter::config {
constexpr static char PROXY_LOG_PATH[] = "log/catter-proxy.log";
#ifdef CATTER_WINDOWS
constexpr static char PROXY_EXE_NAME[] = "catter-proxy.exe";
#else
constexpr static char PROXY_EXE_NAME[] = "catter-proxy";
#endif
};  // namespace catter::config
