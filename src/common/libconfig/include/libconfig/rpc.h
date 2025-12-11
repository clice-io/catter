#pragma once

namespace catter::config::rpc {
#ifdef CATTER_WINDOWS
constexpr char PIPE_NAME[] = R"(\\.\pipe\catter-rpc)";
#else
constexpr char PIPE_NAME[] = "pipe-catter-rpc.sock";
#endif
}  // namespace catter::config::rpc
