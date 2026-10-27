#pragma once

namespace catter::config {
constexpr static char PROXY_LOG_PATH[] = "log/catter-proxy.log";
constexpr static char PROXY_EXE_NAME[] = "catter-proxy";
/// The name catter-proxy has among the wrapper backend's shims, where it starts
/// the program its first argument names as a command of the build: the build
/// command, and each compile CMake runs through it.
constexpr static char LAUNCHER_NAME[] = "catter-launch";
/// The file beside a shim that names the program it stands in for, by its
/// path, when that is not the first program of its name on PATH: one a
/// variable such as CC names.
constexpr static char SHIM_PROGRAM_FILE[] = "program";
};  // namespace catter::config
