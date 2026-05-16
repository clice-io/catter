#pragma once

#include <filesystem>
#include <string>
#include <kota/async/runtime/task.h>

namespace catter::core {
struct CatterConfig;
}

namespace catter::app {

struct ScriptRunConfig {
    std::string script_content;
    std::string script_path;
    std::filesystem::path working_directory;
};

kota::task<> async_run(ScriptRunConfig config);
kota::task<> async_run(const core::CatterConfig& config);

}  // namespace catter::app
