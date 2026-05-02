#include "app_runner.h"

#include <cstdint>
#include <exception>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <cpptrace/exceptions.hpp>
#include <kota/async/runtime/task.h>

#include "app_config.h"
#include "ipc.h"
#include "option.h"
#include "session.h"
#include "config/catter-proxy.h"
#include "js/async.h"
#include "js/capi/type.h"
#include "util/crossplat.h"
#include "util/data.h"

namespace catter::app {
namespace {
class ServiceImpl : public ipc::InjectService {
public:
    ServiceImpl(data::ipcid_t id, const js::CatterRuntime* runtime) : id(id), runtime(runtime) {}

    ~ServiceImpl() override = default;

    kota::task<data::ipcid_t> create(data::ipcid_t parent_id) override {
        this->parent_id = parent_id;
        co_return this->id;
    }

    kota::task<data::action> make_decision(data::command cmd) override {
        auto act = co_await js::on_command(this->id,
                                           js::CommandData{
                                               .cwd = cmd.cwd,
                                               .exe = cmd.executable,
                                               .argv = cmd.args,
                                               .env = cmd.env,
                                               .runtime = *runtime,
                                               .parent = this->parent_id,
                                           });

        switch(act.type()) {
            case js::ActionType::drop: {
                co_return data::action{.type = data::action::DROP, .cmd = {}};
            }
            case js::ActionType::skip: {
                co_return data::action{.type = data::action::INJECT, .cmd = cmd};
            }
            case js::ActionType::modify: {
                auto& tag = act.get<js::ActionType::modify>();
                co_return data::action{
                    .type = data::action::INJECT,
                    .cmd = {
                            .cwd = std::move(tag.data.cwd),
                            .executable = std::move(tag.data.exe),
                            .args = std::move(tag.data.argv),
                            .env = std::move(tag.data.env),
                            }
                };
            }
            default: throw cpptrace::runtime_error("Unhandled action type");
        }
    }

    kota::task<> finish(data::process_result result) override {
        co_await js::on_execution(this->id,
                                  js::ProcessResult{
                                      .code = result.code,
                                      .stdOut = std::move(result.std_out),
                                      .stdErr = std::move(result.std_err),
                                  });
    }

    kota::task<> report_error(data::ipcid_t parent_id, std::string error_msg) override {
        (void)parent_id;
        co_await js::on_command(id, std::unexpected(js::CatterErr{.msg = std::move(error_msg)}));
    }

    struct Factory {
        const js::CatterRuntime* runtime;

        std::unique_ptr<ServiceImpl> operator() (data::ipcid_t id) const {
            return std::make_unique<ServiceImpl>(id, runtime);
        }
    };

private:
    data::ipcid_t id = 0;
    data::ipcid_t parent_id = 0;
    const js::CatterRuntime* runtime = nullptr;
};

kota::task<data::process_result> inject(const js::CatterConfig& config) {
    if(config.buildSystemCommand.empty()) {
        throw cpptrace::runtime_error("buildSystemCommand must not be empty");
    }

    auto proxy_path = util::get_catter_root_path() / config::proxy::EXE_NAME;
    Session::ProcessLaunchPlan launch_plan{
        .cwd = config.buildSystemCommandCwd,
        .executable = proxy_path.string(),
        .args =
            {
                   proxy_path.string(),
                   "-p", "0",
                   "--", },
    };
    util::append_range_to_vector(launch_plan.args, config.buildSystemCommand);

    Session session;
    auto session_plan = Session::make_run_plan(std::move(launch_plan),
                                               ServiceImpl::Factory{.runtime = &config.runtime});

    co_return co_await session.run(std::move(session_plan));
}

kota::task<data::process_result> execute_service(ipc::ServiceMode mode,
                                                 const js::CatterConfig& config) {
    switch(mode) {
        case ipc::ServiceMode::INJECT: {
            co_return co_await inject(config);
        }
        default: {
            throw cpptrace::runtime_error(
                std::format("UnExpected mode: {:0x}", static_cast<uint8_t>(mode)));
        }
    }
    throw cpptrace::runtime_error("Not implemented");
}

}  // namespace

kota::task<> async_run(const core::CatterConfig& config) {
    auto script_content = load_script_content(config.script_path.value());
    js::JsLoop js_loop;
    js::JsLoopScope js_loop_scope(js_loop);
    std::exception_ptr error;

    try {
        co_await js::async_init_qjs({.pwd = config.working_dir->path});
        co_await js::async_run_js_file(script_content, config.script_path.value());

        auto js_config = co_await js::on_start({
            .scriptPath = config.script_path.value(),
            .scriptArgs = config.script_args,
            .buildSystemCommand = config.command.value(),
            .buildSystemCommandCwd = config.working_dir->path.string(),
            .runtime = config.mode->runtime,
            .options = {.log = config.log},
            .execute = true,
        });

        if(js_config.execute) {
            auto process_result = co_await execute_service(config.mode->mode, js_config);

            co_await js::on_finish(js::ProcessResult{
                .code = process_result.code,
                .stdOut = std::move(process_result.std_out),
                .stdErr = std::move(process_result.std_err),
            });
        }
    } catch(...) {
        error = std::current_exception();
    }

    if(error) {
        std::rethrow_exception(error);
    }
}

}  // namespace catter::app
