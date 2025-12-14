#pragma once

namespace catter::config::main {
#ifdef CATTER_LINUX
constexpr const inline static char LOG_PATH_REL[] = "log/catter-main.log";
#endif
#ifdef CATTER_WINDOWS
#error "TODO"
#endif
};  // namespace catter::config::main
