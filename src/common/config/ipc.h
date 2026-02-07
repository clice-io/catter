#pragma once

namespace catter::config::ipc {
#ifdef CATTER_WINDOWS
constexpr char PIPE_NAME[] = R"(\\.\pipe\catter-ipc)";
#else
constexpr char PIPE_NAME[] = "pipe-catter-ipc.sock";
#endif
}  // namespace catter::config::ipc
