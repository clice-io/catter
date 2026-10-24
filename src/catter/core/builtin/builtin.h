#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "handler.h"

namespace catter::builtin {

/// The built-in `name` names, `script::cdb`, `script::cmd-tree`, `script::target-tree` or
/// `script::inspect`, given `args`; none for a name of no built-in.
std::unique_ptr<core::Handler> make(std::string_view name, std::vector<std::string> args);

}  // namespace catter::builtin
