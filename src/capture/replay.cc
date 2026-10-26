module;

#include "modules/prelude.h"

module catter;

import :capture.replay;

namespace catter {

namespace {

/// How a command or the build ended.
struct ReplayResult {
    std::int64_t code;
};

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
    data::ipcid_t id;
    std::optional<data::ipcid_t> parent;
    std::optional<std::string> cwd;
    std::string exe;
    std::vector<std::string> argv;
    std::optional<std::vector<std::string>> env;
    std::optional<ReplayResult> execution;
    std::optional<ReplayError> error;
};

/// Top-level replay document (schema version 1).
///
/// `cwd`, relative to the replay file's directory, is where the commands with a
/// relative `cwd` of their own ran; `finish` is how the build ended.
struct ReplayFile {
    std::int64_t version;
    std::optional<std::string> cwd;
    std::vector<ReplayEvent> events;
    std::optional<ReplayResult> finish;
};

void validate_replay_file(const ReplayFile& replay) {
    if(replay.version != 1) {
        throw cpptrace::runtime_error(
            std::format("Unsupported replay file version: {}", replay.version));
    }

    for(const auto& event: replay.events) {
        if(event.type.has_value() && *event.type != "command") {
            throw cpptrace::runtime_error(
                std::format("Unsupported replay event type: `{}`", *event.type));
        }
        if(event.error.has_value() && event.execution.has_value()) {
            throw cpptrace::runtime_error(
                std::format("Replay event {} has both 'error' and 'execution'", event.id));
        }
    }
}

/// A replay file's `parent` of 0, like an absent one, is no parent.
std::optional<data::ipcid_t> parent_of(std::optional<data::ipcid_t> parent) {
    if(parent == 0) {
        return std::nullopt;
    }
    return parent;
}

data::process_result to_process_result(const ReplayResult& result) {
    return {.code = result.code};
}

ReplayFile parse_replay_file(std::string_view path) {
    std::string content;
    if(auto error = kota::fs::read_file(content, path)) {
        throw cpptrace::runtime_error(
            std::format("Failed to open replay file: {}: {}", path, error.message()));
    }

    auto replay = kota::codec::json::from_string<ReplayFile>(content);
    if(!replay.has_value()) {
        throw cpptrace::runtime_error(
            std::format("Replay file is not valid: {}: {}", path, replay.error().message));
    }
    validate_replay_file(*replay);
    return std::move(*replay);
}

}  // namespace

kota::task<kota::process::exit_status> replay_build(std::string_view path, Handler& handler) {
    const auto replay = parse_replay_file(path);
    std::string base(kota::fs::path::parent(path));
    kota::fs::path::push(base, replay.cwd.value_or("."));
    kota::fs::path::normalize(base);
    for(const auto& event: replay.events) {
        if(event.error.has_value()) {
            co_await handler.on_error(event.id,
                                      parent_of(event.error->parent),
                                      event.error->message);
            continue;
        }
        auto cwd = base;
        kota::fs::path::push(cwd, event.cwd.value_or("."));
        kota::fs::path::normalize(cwd);
        co_await handler.on_command(event.id,
                                    parent_of(event.parent),
                                    data::command{
                                        .cwd = std::move(cwd),
                                        .executable = event.exe,
                                        .args = event.argv,
                                        .env = event.env.value_or(std::vector<std::string>{}),
                                    });
        if(event.execution.has_value()) {
            co_await handler.on_exit(event.id, to_process_result(*event.execution));
        }
    }

    co_return kota::process::exit_status{.code = replay.finish ? replay.finish->code : 0};
}

}  // namespace catter
