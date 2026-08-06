#include "replay.h"

#include <exception>
#include <expected>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <cpptrace/exceptions.hpp>
#include <kota/async/io/loop.h>

#include "js/builtin_files.h"
#include "js/capi/bridge.h"
#include "js/js.h"
#include "js/qjs.h"

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

std::string load_script_source(const ReplayConfig& config) {
    if(config.script.starts_with("script::")) {
        const auto source = js::load_builtin_script(config.script);
        if(source.empty()) {
            throw cpptrace::runtime_error("Unknown builtin script '" + config.script + "'");
        }
        return std::string(source);
    }

    std::ifstream input(config.script, std::ios::binary);
    if(!input) {
        throw cpptrace::runtime_error("Failed to open script file: " + config.script);
    }
    return std::string(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
}

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
                std::format("Unsupported replay event type: '{}'", *event.type));
        }
        if(event.error.has_value() && event.execution.has_value()) {
            throw cpptrace::runtime_error(
                std::format("Replay event {} has both 'error' and 'execution'", event.id));
        }
    }
}

js::CatterConfig to_catter_config(const ReplayConfig& config, const ReplayFile& replay) {
    return {
        .scriptPath = config.script,
        .scriptArgs = config.script_args,
        .buildSystemCommand = replay.build_system_command.value_or(config.build_system_command),
        .buildSystemCommandCwd = config.working_directory.string(),
        .runtime = config.runtime,
        .options = config.options,
        .execute = config.execute,
    };
}

std::string command_cwd(const ReplayConfig& config, const ReplayEvent& event) {
    if(!event.cwd.has_value()) {
        return config.working_directory.string();
    }

    fs::path cwd = *event.cwd;
    if(!cwd.is_absolute()) {
        cwd = config.working_directory / cwd;
    }
    return cwd.lexically_normal().string();
}

std::expected<js::CommandData, js::CatterErr> to_command_data(const ReplayConfig& config,
                                                              const ReplayEvent& event) {
    if(event.error.has_value()) {
        return std::unexpected(js::CatterErr{
            .msg = event.error->message,
            .parent = event.error->parent,
        });
    }

    return js::CommandData{
        .cwd = command_cwd(config, event),
        .exe = event.exe,
        .argv = event.argv,
        .env = event.env.value_or(std::vector<std::string>{}),
        .runtime = config.runtime,
        .parent = event.parent,
    };
}

kota::task<> replay_task(ReplayConfig config, const ReplayFile& replay) {
    js::RuntimeScope runtime;
    std::exception_ptr error;
    try {
        runtime.start({.pwd = config.working_directory});

        auto source = load_script_source(config);
        co_await js::run_script(source, config.script);

        auto script_config = co_await js::on_start(to_catter_config(config, replay));
        if(script_config.execute) {
            for(const auto& event: replay.events) {
                co_await js::on_command(event.id, to_command_data(config, event));
                if(event.execution.has_value()) {
                    co_await js::on_execution(event.id, *event.execution);
                }
            }

            co_await js::on_finish(replay.finish.value_or(js::ProcessResult{.code = 0}));
        }

    } catch(...) {
        error = std::current_exception();
    }

    co_await runtime.stop();

    if(error) {
        std::rethrow_exception(error);
    }
    co_return;
}

}  // namespace

ReplayFile parse_replay_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if(!input) {
        throw cpptrace::runtime_error("Failed to open replay file: " + path.string());
    }
    std::string content(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});

    auto runtime = qjs::Runtime::create();
    auto ctx = runtime.context();
    auto value = qjs::json::parse(content, ctx);
    auto replay = js::make_reflected_object<ReplayFile>(value.as<qjs::Object>());
    validate_replay_file(replay);
    return replay;
}

void ReplayRunner::run(ReplayConfig config, const ReplayFile& replay) {
    auto task = replay_task(std::move(config), replay);

    kota::event_loop loop;
    loop.schedule(task);
    loop.run();
    task.result();
}

}  // namespace catter::core
