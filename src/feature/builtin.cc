#include "feature/builtin.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "feature/handlers.h"
#include "session/handler.h"

namespace catter::feature {

std::unique_ptr<Handler> make(std::string_view name, std::vector<std::string> args) {
    if(name == "cdb") {
        return make_cdb(std::move(args));
    }
    if(name == "cmd-tree") {
        return make_cmd_tree(std::move(args));
    }
    if(name == "target-tree") {
        return make_target_tree(std::move(args));
    }
    if(name == "inspect") {
        return make_inspect(std::move(args));
    }
    return nullptr;
}

}  // namespace catter::feature
