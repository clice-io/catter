module;

#include "modules/prelude.h"

module catter.command:executable;

namespace catter {

/// What follows a program's name for its version: `-12`, `_13.2.0`, `18`, MacPorts' `-mp-14`.
constexpr inline std::string_view version_suffix = R"((?:(?:-mp)?[-_]?\d+(?:[._-][0-9a-zA-Z]+)*)?)";

/// What matches an executable whose name is what `name` matches, followed by what `suffix`
/// matches: perhaps after a directory, before `.exe`, without case on Windows.
std::regex executable_pattern(std::string_view name, std::string_view suffix);

}  // namespace catter
