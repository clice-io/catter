#pragma once
#include <string>
#include <string_view>

namespace catter {

/// The path of `name` in catter's data directory: CATTER_HOME when it is set
/// (an absolute path, or it throws), else ~/.catter, or catter under the local
/// application data on Windows.
std::string data_path(std::string_view name);

/// The path of `name` beside this program, where catter's programs and hook
/// libraries are installed together.
std::string program_path(std::string_view name);

/// The file the system starts for program `name`: found as execvp finds it,
/// or on Windows as CreateProcess does; `name` itself when there is none, to
/// fail to start as it would without catter.
std::string resolve_program(std::string_view name);

}  // namespace catter
