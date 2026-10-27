#pragma once
#include <string>
#include <string_view>

namespace catter {

/// The file the system starts for program `name` from directory `directory`,
/// this process's current one when empty: found as execvp finds it, or on
/// Windows as CreateProcess does; `name` itself when there is none, to fail to
/// start as it would without catter.
std::string resolve_program(std::string_view name, std::string_view directory = {});

}  // namespace catter
