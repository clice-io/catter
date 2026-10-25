#include "script_handler.h"

#include <expected>
#include <format>
#include <utility>
#include <cpptrace/exceptions.hpp>

#include "js.h"

namespace catter::js {

namespace {

/// The runtime scripts see: inject's, which replay plays a build back as.
const CatterRuntime runtime{
    .supportActions = {ActionType::drop, ActionType::skip, ActionType::abort, ActionType::modify},
    .type = CatterRuntime::Type::inject,
    .supportParentId = true,
};

CatterOptions::StdioMode to_js(core::StdioMode mode) {
    switch(mode) {
        case core::StdioMode::inherit: return CatterOptions::StdioMode::inherit;
        case core::StdioMode::capture: return CatterOptions::StdioMode::capture;
    }
    std::unreachable();
}

ProcessResult to_js(data::process_result result) {
    return {
        .code = result.code,
        .stdOut = std::move(result.std_out),
        .stdErr = std::move(result.std_err),
    };
}

core::StdioMode to_core(CatterOptions::StdioMode mode) {
    switch(mode) {
        case CatterOptions::StdioMode::inherit: return core::StdioMode::inherit;
        case CatterOptions::StdioMode::capture: return core::StdioMode::capture;
    }
    std::unreachable();
}

core::Action to_core(data::ipcid_t id, Action action) {
    switch(action.type()) {
        case ActionType::skip: return {.type = core::Action::Type::Skip};
        case ActionType::drop: return {.type = core::Action::Type::Drop};
        case ActionType::abort: {
            throw cpptrace::runtime_error(
                std::format("The script aborted the build at command {}", id));
        }
        case ActionType::modify: {
            auto& data = action.get<ActionType::modify>().data;
            return {
                .type = core::Action::Type::Modify,
                .command =
                    {
                              .cwd = std::move(data.cwd),
                              .executable = std::move(data.exe),
                              .args = std::move(data.argv),
                              .env = std::move(data.env),
                              },
            };
        }
    }
    std::unreachable();
}

}  // namespace

ScriptHandler::ScriptHandler(std::string script_path,
                             std::vector<std::string> script_args,
                             bool log) :
    script_path(std::move(script_path)), script_args(std::move(script_args)), log(log) {}

kota::task<std::optional<core::Build>> ScriptHandler::on_start(core::Build build) {
    auto config = co_await js::on_start({
        .scriptPath = script_path,
        .scriptArgs = script_args,
        .buildSystemCommand = std::move(build.command),
        .buildSystemCommandCwd = std::move(build.cwd),
        .runtime = runtime,
        .options = {.log = log, .stdioMode = to_js(build.stdio_mode)},
        .execute = true,
    });
    if(!config.execute) {
        co_return std::nullopt;
    }
    co_return core::Build{
        .command = std::move(config.buildSystemCommand),
        .cwd = std::move(config.buildSystemCommandCwd),
        .stdio_mode = to_core(config.options.stdioMode),
    };
}

kota::task<core::Action> ScriptHandler::on_command(data::ipcid_t id,
                                                   std::optional<data::ipcid_t> parent,
                                                   data::command command) {
    auto action = co_await js::on_command(id,
                                          CommandData{
                                              .cwd = std::move(command.cwd),
                                              .exe = std::move(command.executable),
                                              .argv = std::move(command.args),
                                              .env = std::move(command.env),
                                              .runtime = runtime,
                                              .parent = parent,
                                          });
    co_return to_core(id, std::move(action));
}

kota::task<> ScriptHandler::on_error(data::ipcid_t id,
                                     std::optional<data::ipcid_t> parent,
                                     std::string message) {
    co_await js::on_command(
        id,
        std::unexpected(CatterErr{.msg = std::move(message), .parent = parent}));
}

kota::task<> ScriptHandler::on_exit(data::ipcid_t id, data::process_result result) {
    co_await js::on_execution(id, to_js(std::move(result)));
}

kota::task<> ScriptHandler::on_finish(data::process_result result) {
    co_await js::on_finish(to_js(std::move(result)));
}

}  // namespace catter::js
