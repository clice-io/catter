#pragma once
#include <string>
#include <string_view>
#include <kota/fs/path.h>

namespace catter {

/// catter-proxy, which catter installs beside itself.
constexpr char PROXY[] = "catter-proxy";

/// The name catter-proxy has among the wrapper backend's shims, where it starts
/// the program its first argument names as a command of the build: the build
/// command, and each compile CMake runs through it.
constexpr char LAUNCHER[] = "catter-launch";

/// The file in a directory of shims that stands for another directory, which
/// names that directory: a shim there runs the program of its name in it.
constexpr char SHIM_DIRECTORY_FILE[] = ".catter-directory";

/// The path of `name` in catter's data directory: CATTER_HOME when it is set
/// (an absolute path, or it throws), else ~/.catter, or catter under the local
/// application data on Windows.
std::string data_path(std::string_view name);

/// The path of `name` in the data directory of the session listening on
/// `pipe`, where its socket is: the session's, though a build step changed
/// HOME or CATTER_HOME. On Windows, whose pipes are in no directory, this
/// process's.
inline std::string data_path([[maybe_unused]] std::string_view pipe, std::string_view name) {
#ifdef CATTER_WINDOWS
    return data_path(name);
#else
    std::string path(kota::fs::path::parent(pipe));
    kota::fs::path::push(path, name);
    return path;
#endif
}

/// The path of `name` beside this program, where catter's programs and hook
/// libraries are installed together.
std::string program_path(std::string_view name);

}  // namespace catter
