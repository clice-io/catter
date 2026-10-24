#pragma once

#include <optional>
#include <string>
#include <vector>
#include <kota/async/runtime/task.h>

#include "handler.h"
#include "util/data.h"

namespace catter::js {

/// Hands the session's events to the callbacks of the running script.
class ScriptHandler final : public core::Handler {
public:
    ScriptHandler(std::string script_path, std::vector<std::string> script_args, bool log);

    kota::task<std::optional<core::Build>> on_start(core::Build build) override;

    kota::task<core::Action> on_command(data::ipcid_t id,
                                        std::optional<data::ipcid_t> parent,
                                        data::command command) override;

    kota::task<> on_error(data::ipcid_t id,
                          std::optional<data::ipcid_t> parent,
                          std::string message) override;

    kota::task<> on_exit(data::ipcid_t id, data::process_result result) override;

    kota::task<> on_finish(data::process_result result) override;

private:
    std::string script_path;

    std::vector<std::string> script_args;

    bool log;
};

}  // namespace catter::js
