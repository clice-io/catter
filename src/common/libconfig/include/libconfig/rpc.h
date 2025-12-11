#pragma once

namespace catter::config::rpc {
#ifdef CATTER_WINDOWS
constexpr const inline static char PIPE_NAME[] = R"(\\.\pipe\catter-rpc)";
#else
constexpr const inline static char PIPE_NAME[] = "pipe-catter-rpc.sock";
#endif
}