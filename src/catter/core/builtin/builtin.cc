#include "builtin/builtin.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "handler.h"
#include "builtin/handlers.h"

namespace catter::builtin {

std::unique_ptr<core::Handler> make(std::string_view name, std::vector<std::string> args) {
    if(name == "script::cdb") {
        return make_cdb(std::move(args));
    }
    if(name == "script::cmd-tree") {
        return make_cmd_tree(std::move(args));
    }
    if(name == "script::target-tree") {
        return make_target_tree(std::move(args));
    }
    if(name == "script::inspect") {
        return make_inspect(std::move(args));
    }
    return nullptr;
}

}  // namespace catter::builtin
