#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "js/capi/type.h"

namespace catter::core {

/**
 * Capture-failure payload of a replayed command, mirroring `js::CatterErr`
 * with a JSON-friendly `message` key.
 */
struct ReplayError {
    std::string message;
    std::optional<int64_t> parent;
};

/**
 * One command event of a replayed build.
 *
 * When `error` is set the command is delivered as a capture failure;
 * otherwise `execution`, when set, is delivered to on_execution after the
 * command is processed. `parent` of 0 means no parent command.
 */
struct ReplayEvent {
    std::optional<std::string> type;  // "command"
    uint32_t id = 0;
    std::optional<int64_t> parent;
    std::optional<std::string> cwd;
    std::string exe;
    std::vector<std::string> argv;
    std::optional<std::vector<std::string>> env;
    std::optional<js::ProcessResult> execution;
    std::optional<ReplayError> error;
};

/**
 * Top-level replay document (schema version 1).
 *
 * `cwd` is resolved by the caller against the replay file directory when it
 * is relative; `build_system_command` and `finish` override the runner's
 * defaults when present.
 */
struct ReplayFile {
    std::optional<int64_t> version;
    std::optional<std::string> name;
    std::optional<std::string> cwd;
    std::optional<std::vector<std::string>> build_system_command;
    std::vector<ReplayEvent> events;
    std::optional<js::ProcessResult> finish;
};

struct ReplayConfig {
    std::string script;  // "script::cdb" or a script file path.
    std::vector<std::string> script_args;
    std::vector<std::string> build_system_command;
    std::filesystem::path working_directory;
    js::CatterOptions options{
        .log = false,
        .stdioMode = js::CatterOptions::StdioMode::inherit,
    };
    js::CatterRuntime runtime{
        .supportActions = {js::ActionType::skip,
                           js::ActionType::drop,
                           js::ActionType::abort,
                           js::ActionType::modify},
        .type = js::CatterRuntime::Type::inject,
        .supportParentId = true,
    };
    bool execute = true;
};

/**
 * Parses a replay JSON document with the embedded QuickJS runtime (no
 * third-party JSON dependency) and validates the schema.
 */
ReplayFile parse_replay_file(const std::filesystem::path& path);

/**
 * Replays a whole build against the embedded QuickJS runtime.
 *
 * Loads the script (builtin or file), then drives the service lifecycle the
 * same way the real runtime driver does: on_start, one on_command /
 * on_execution pair per command, then on_finish. Exceptions thrown by the
 * script (e.g. abort-on-command-failure) propagate to the caller.
 */
class ReplayRunner {
public:
    void run(ReplayConfig config, const ReplayFile& replay);
};

}  // namespace catter::core
