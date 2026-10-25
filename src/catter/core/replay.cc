#include "replay.h"

#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <cpptrace/exceptions.hpp>

#include "js/capi/bridge.h"
#include "js/qjs.h"
#include "util/encoding.h"

// The generic `Bridge<std::vector<T>>` only supports primitive elements, so
// vector<ReplayEvent> needs an explicit specialization. JS arrays expose
// indexed elements as string properties, which `qjs::Object::operator[]`
// can read directly.
namespace catter::js {

template <>
struct Bridge<std::vector<catter::core::ReplayEvent>> {
    static std::vector<catter::core::ReplayEvent> from_js(const qjs::Value& value) {
        auto array = value.as<qjs::Object>();
        auto length = array["length"].as<uint32_t>();
        std::vector<catter::core::ReplayEvent> events;
        events.reserve(length);
        for(uint32_t index = 0; index < length; ++index) {
            auto element = array[std::to_string(index)];
            events.push_back(
                make_reflected_object<catter::core::ReplayEvent>(element.as<qjs::Object>()));
        }
        return events;
    }
};

}  // namespace catter::js

namespace catter::core {
namespace fs = std::filesystem;

namespace {

void validate_replay_file(const ReplayFile& replay) {
    if(!replay.version.has_value()) {
        throw cpptrace::runtime_error("Replay file is missing required field 'version'");
    }
    if(*replay.version != 1) {
        throw cpptrace::runtime_error(
            std::format("Unsupported replay file version: {}", *replay.version));
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

std::string command_cwd(const fs::path& base, const ReplayEvent& event) {
    if(!event.cwd.has_value()) {
        return util::to_utf8(base);
    }

    fs::path cwd = util::from_utf8(*event.cwd);
    if(!cwd.is_absolute()) {
        cwd = base / cwd;
    }
    cwd = cwd.lexically_normal();
    return util::to_utf8(cwd.has_filename() ? cwd : cwd.parent_path());
}

/// A replay file's `parent` of 0, like an absent one, is no parent.
std::optional<data::ipcid_t> parent_of(std::optional<data::ipcid_t> parent) {
    if(parent == 0) {
        return std::nullopt;
    }
    return parent;
}

data::process_result to_process_result(const js::ProcessResult& result) {
    return {
        .code = result.code,
        .std_out = result.stdOut,
        .std_err = result.stdErr,
    };
}

}  // namespace

ReplayFile parse_replay_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if(!input) {
        throw cpptrace::runtime_error("Failed to open replay file: " + util::to_utf8(path));
    }
    std::string content(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});

    auto runtime = qjs::Runtime::create();
    auto ctx = runtime.context();
    auto value = qjs::json::parse(content, ctx);
    auto replay = js::make_reflected_object<ReplayFile>(value.as<qjs::Object>());
    validate_replay_file(replay);
    return replay;
}

kota::task<data::process_result> replay_build(const fs::path& path, Handler& handler) {
    const auto replay = parse_replay_file(path);
    auto base = (path.parent_path() / util::from_utf8(replay.cwd.value_or("."))).lexically_normal();
    if(!base.has_filename()) {
        base = base.parent_path();
    }
    for(const auto& event: replay.events) {
        if(event.error.has_value()) {
            co_await handler.on_error(event.id,
                                      parent_of(event.error->parent),
                                      event.error->message);
            continue;
        }
        co_await handler.on_command(event.id,
                                    parent_of(event.parent),
                                    data::command{
                                        .cwd = command_cwd(base, event),
                                        .executable = event.exe,
                                        .args = event.argv,
                                        .env = event.env.value_or(std::vector<std::string>{}),
                                    });
        if(event.execution.has_value()) {
            co_await handler.on_exit(event.id, to_process_result(*event.execution));
        }
    }

    co_return to_process_result(replay.finish.value_or(js::ProcessResult{.code = 0}));
}

}  // namespace catter::core
