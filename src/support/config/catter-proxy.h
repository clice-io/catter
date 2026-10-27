#pragma once

namespace catter::config {
constexpr static char PROXY_LOG_PATH[] = "log/catter-proxy.log";
constexpr static char PROXY_EXE_NAME[] = "catter-proxy";
/// The name catter-proxy has among the wrapper backend's shims, where it starts
/// the program its first argument names as a command of the build: the build
/// command, and each compile CMake runs through it.
constexpr static char LAUNCHER_NAME[] = "catter-launch";
/// The file in a directory of shims that stands for another directory, which
/// names that directory: a shim there runs the program of its name in it.
constexpr static char SHIM_DIRECTORY_FILE[] = ".catter-directory";
};  // namespace catter::config
