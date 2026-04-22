#include "app_config.h"

#include <format>
#include <fstream>
#include <iterator>
#include <string_view>
#include <unordered_map>
#include <cpptrace/exceptions.hpp>

namespace catter::app {

std::string load_script_content(const std::string& script_path) {
    const static std::unordered_map<std::string_view, std::string_view> internal_scripts = {
        {"script::cdb",
         R"(
    import { scripts, service } from "catter";
    service.register(new scripts.CDB());
    )"},
        {"script::cmd-tree",
         R"(
    import { scripts, service } from "catter";
    service.register(new scripts.CmdTree());
    )"},
        {"script::target-tree",
         R"(
    import { scripts, service } from "catter";
    service.register(new scripts.TargetTree());
    )"}
    };

    if(auto it = internal_scripts.find(script_path); it != internal_scripts.end()) {
        return std::string(it->second);
    }

    std::ifstream ifs{script_path};
    if(!ifs.good()) {
        throw cpptrace::runtime_error(std::format("Failed to open script file: {}", script_path));
    }

    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

}  // namespace catter::app
