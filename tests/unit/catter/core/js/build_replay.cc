#include "build_replay.h"

#include <exception>
#include <expected>
#include <fstream>
#include <iterator>
#include <utility>
#include <cpptrace/exceptions.hpp>
#include <kota/async/io/loop.h>

#include "js/builtin_files.h"
#include "js/js.h"

namespace catter::tests::js {

namespace {

std::string load_script_source(const BuildReplayConfig& config) {
    if(config.script.starts_with("script::")) {
        const auto source = catter::js::load_builtin_script(config.script);
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

catter::js::CatterConfig to_catter_config(const BuildReplayConfig& config) {
    return {
        .scriptPath = config.script,
        .scriptArgs = config.script_args,
        .buildSystemCommand = config.build_system_command,
        .buildSystemCommandCwd = config.working_directory.string(),
        .runtime = config.runtime,
        .options = config.options,
        .execute = config.execute,
    };
}

std::expected<catter::js::CommandData, catter::js::CatterErr>
    to_command_data(const BuildReplayConfig& config, const ReplayCommand& command) {
    if(command.error.has_value()) {
        return std::unexpected(*command.error);
    }

    return catter::js::CommandData{
        .cwd = command.cwd,
        .exe = command.exe,
        .argv = command.argv,
        .env = command.env,
        .runtime = config.runtime,
        .parent = command.parent == 0 ? std::optional<int64_t>{} : command.parent,
    };
}

kota::task<> replay_task(BuildReplayConfig config,
                         std::vector<ReplayCommand> commands,
                         catter::js::ProcessResult finish_result) {
    catter::js::RuntimeScope runtime;
    std::exception_ptr error;
    try {
        runtime.start({.pwd = config.working_directory});

        auto source = load_script_source(config);
        co_await catter::js::run_script(source, config.script);

        auto script_config = co_await catter::js::on_start(to_catter_config(config));
        if(!script_config.execute) {
            co_return;
        }

        for(const auto& command: commands) {
            auto data = to_command_data(config, command);
            auto action = co_await catter::js::on_command(command.id, std::move(data));
            (void)action;
            if(command.execution.has_value()) {
                co_await catter::js::on_execution(command.id, *command.execution);
            }
        }

        co_await catter::js::on_finish(std::move(finish_result));
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

void BuildReplay::run(BuildReplayConfig config,
                      std::vector<ReplayCommand> commands,
                      catter::js::ProcessResult finish_result) {
    auto task = replay_task(std::move(config), std::move(commands), std::move(finish_result));

    kota::event_loop loop;
    loop.schedule(task);
    loop.run();
    task.result();
}

}  // namespace catter::tests::js
