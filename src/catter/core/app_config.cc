#include "app_config.h"

#include <format>
#include <fstream>
#include <iterator>
#include <string_view>
#include <unordered_map>

namespace catter::app {

StartupConfig to_startup_config(const core::Option::CatterOption& opt) {
    return StartupConfig{
        .log = true,
        .mode = *opt.mode,
        .script_path = *opt.script_path,
        .script_args = opt.script_args.has_value() ? *opt.script_args : std::vector<std::string>{},
        .build_system_command = *opt.args,
        .working_dir = opt.working_dir.has_value() ? std::filesystem::absolute(*opt.working_dir)
                                                   : std::filesystem::current_path(),
    };
}

RuntimePlan build_runtime_plan(const StartupConfig& config) {
    struct ModeMeta {
        ipc::ServiceMode mode;
        js::CatterRuntime runtime;
    };

    const static std::unordered_map<std::string_view, ModeMeta> mode_map = {
        {"inject",
         {.mode = ipc::ServiceMode::INJECT,
          .runtime = {
              .supportActions = {js::ActionType::drop,
                                 js::ActionType::skip,
                                 js::ActionType::modify},
              .supportEvents = {js::EventType::finish},
              .type = js::CatterRuntime::Type::inject,
              .supportParentId = true,
          }}}
    };

    auto it = mode_map.find(config.mode);
    if(it == mode_map.end()) {
        throw std::runtime_error(std::format("Unsupported mode: {}", config.mode));
    }

    return RuntimePlan{
        .log = config.log,
        .mode = it->second.mode,
        .script_path = config.script_path,
        .script_args = config.script_args,
        .build_system_command = config.build_system_command,
        .working_dir = config.working_dir,
        .runtime = &it->second.runtime,
    };
}

std::string load_script_content(const std::string& script_path) {
    const static std::unordered_map<std::string_view, std::string_view> internal_scripts = {
        {"script::cdb",
         R"(
    import { scripts, service } from "catter";
    service.register(new scripts.CDB());
    )"}
    };

    if(auto it = internal_scripts.find(script_path); it != internal_scripts.end()) {
        return std::string(it->second);
    }

    std::ifstream ifs{script_path};
    if(!ifs.good()) {
        throw std::runtime_error(std::format("Failed to open script file: {}", script_path));
    }

    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

}  // namespace catter::app
