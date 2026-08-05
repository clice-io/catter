#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "js/capi/type.h"

namespace catter::tests::js {

/**
 * One command event of a replayed build.
 *
 * When `error` is set the command is delivered as a capture failure;
 * otherwise `execution`, when set, is delivered to on_execution after the
 * command is processed.
 */
struct ReplayCommand {
    uint32_t id = 0;
    uint32_t parent = 0;  // 0 means no parent command.
    std::string cwd;
    std::string exe;
    std::vector<std::string> argv;
    std::vector<std::string> env;
    std::optional<catter::js::CatterErr> error;
    std::optional<catter::js::ProcessResult> execution;
};

struct BuildReplayConfig {
    std::string script;  // "script::cdb" or a script file path.
    std::vector<std::string> script_args;
    std::vector<std::string> build_system_command;
    std::filesystem::path working_directory;
    catter::js::CatterOptions options{
        .log = false,
        .stdioMode = catter::js::CatterOptions::StdioMode::inherit,
    };
    catter::js::CatterRuntime runtime{
        .supportActions = {catter::js::ActionType::skip,
                           catter::js::ActionType::drop,
                           catter::js::ActionType::abort,
                           catter::js::ActionType::modify},
        .type = catter::js::CatterRuntime::Type::inject,
        .supportParentId = true,
    };
    bool execute = true;
};

/**
 * Replays a whole build against the embedded QuickJS runtime.
 *
 * Loads the script (builtin or file), then drives the service lifecycle the
 * same way the real runtime driver does: on_start, one on_command /
 * on_execution pair per command, then on_finish. Exceptions thrown by the
 * script (e.g. abort-on-command-failure) propagate to the caller.
 */
class BuildReplay {
public:
    void run(BuildReplayConfig config,
             std::vector<ReplayCommand> commands,
             catter::js::ProcessResult finish_result);
};

}  // namespace catter::tests::js
