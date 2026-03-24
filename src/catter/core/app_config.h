#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "capi/type.h"
#include "ipc.h"
#include "opt/main/option.h"

namespace catter::app {

struct StartupConfig {
    bool log;
    std::string mode;
    std::string script_path;
    std::vector<std::string> script_args;
    std::vector<std::string> build_system_command;
    std::filesystem::path working_dir;
};

struct RuntimePlan {
    bool log;
    ipc::ServiceMode mode;
    std::string script_path;
    std::vector<std::string> script_args;
    std::vector<std::string> build_system_command;
    std::filesystem::path working_dir;
    const js::CatterRuntime* runtime;
};

StartupConfig to_startup_config(const core::Option::CatterOption& opt);

RuntimePlan build_runtime_plan(const StartupConfig& config);

std::string load_script_content(const std::string& script_path);

}  // namespace catter::app
