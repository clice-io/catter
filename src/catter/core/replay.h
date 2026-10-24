#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <kota/async/runtime/task.h>

#include "handler.h"
#include "js/capi/type.h"
#include "util/data.h"

namespace catter::core {

/// Why catter could not capture a replayed command.
struct ReplayError {
    std::string message;
    std::optional<data::ipcid_t> parent;
};

/// One command event of a replayed build.
///
/// When `error` is set the command is delivered as a capture failure;
/// otherwise `execution`, when set, is delivered to on_exit after the
/// command is processed. `parent` of 0 means no parent command.
struct ReplayEvent {
    std::optional<std::string> type;  // "command"
    data::ipcid_t id = 0;
    std::optional<data::ipcid_t> parent;
    std::optional<std::string> cwd;
    std::string exe;
    std::vector<std::string> argv;
    std::optional<std::vector<std::string>> env;
    std::optional<js::ProcessResult> execution;
    std::optional<ReplayError> error;
};

/// Top-level replay document (schema version 1).
///
/// `cwd`, relative to the replay file's directory, is where the commands with a
/// relative `cwd` of their own ran; `finish` is how the build ended.
struct ReplayFile {
    std::optional<int64_t> version;
    std::optional<std::string> name;
    std::optional<std::string> cwd;
    std::vector<ReplayEvent> events;
    std::optional<js::ProcessResult> finish;
};

/// Parses a replay JSON document with the embedded QuickJS runtime (no
/// third-party JSON dependency) and validates the schema.
ReplayFile parse_replay_file(const std::filesystem::path& path);

/// Plays the build a replay file recorded to the handler, as the inject mode
/// delivers a real one: each command to on_command, or to on_error when the
/// event has an error, and its result, when the event has one, to on_exit.
///
/// @return how the build ended: the file's `finish`, else exit 0
kota::task<data::process_result> replay_build(const std::filesystem::path& path, Handler& handler);

}  // namespace catter::core
