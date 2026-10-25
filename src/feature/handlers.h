#pragma once

#include <memory>
#include <string>
#include <vector>

#include "session/handler.h"

namespace catter::builtin {

std::unique_ptr<core::Handler> make_cdb(std::vector<std::string> args);
std::unique_ptr<core::Handler> make_cmd_tree(std::vector<std::string> args);
std::unique_ptr<core::Handler> make_target_tree(std::vector<std::string> args);
std::unique_ptr<core::Handler> make_inspect(std::vector<std::string> args);

}  // namespace catter::builtin
