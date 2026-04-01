#include "app_runner.h"

#include <cstdint>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "app_config.h"
#include "capi/type.h"
#include "ipc.h"
#include "js.h"
#include "qjs.h"
#include "session.h"
#include "config/catter-proxy.h"
#include "util/crossplat.h"
#include "util/data.h"

namespace catter::app {
namespace {

class ServiceImpl : public ipc::InjectService {
public:
    ServiceImpl(data::ipcid_t id, const js::CatterRuntime* runtime) : id(id), runtime(runtime) {}

    ~ServiceImpl() override = default;

    data::ipcid_t create(data::ipcid_t parent_id) override {
        this->parent_id = parent_id;
        return this->id;
    }

    data::action make_decision(data::command cmd) override {
        auto act = js::on_command(this->id,
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
                return data::action{.type = data::action::DROP, .cmd = {}};
            }
            case js::ActionType::skip: {
                return data::action{.type = data::action::INJECT, .cmd = cmd};
            }
            case js::ActionType::modify: {
                auto& tag = act.get<js::ActionType::modify>();
                return data::action{
                    .type = data::action::INJECT,
                    .cmd = {
                            .cwd = std::move(tag.data.cwd),
                            .executable = std::move(tag.data.exe),
                            .args = std::move(tag.data.argv),
                            .env = std::move(tag.data.env),
                            }
                };
            }
            default: throw std::runtime_error("Unhandled action type");
        }
    }

    void finish(data::process_result result) override {
        if(!result.std_out.empty() || !result.std_err.empty()) {
            js::on_execution(this->id,
                             js::Tag<js::EventType::output>{
                                 .stdOut = std::move(result.std_out),
                                 .stdErr = std::move(result.std_err),
                                 .code = result.code,
                             });
        }

        js::on_execution(this->id, js::Tag<js::EventType::finish>{.code = result.code});
    }

    void report_error(data::ipcid_t parent_id, std::string error_msg) override {
        js::on_command(id, std::unexpected(js::CatterErr{.msg = std::move(error_msg)}));
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

int64_t inject(const js::CatterConfig& config) {

    auto proxy_path = util::get_catter_root_path() / config::proxy::EXE_NAME;
    Session::ProcessLaunchPlan launch_plan{
        .executable = proxy_path.string(),
        .args =
            {
                   proxy_path.string(),
                   "-p", "0",
                   "--exec", config.buildSystemCommand.front(),
                   "--", },
    };
    append_range_to_vector(launch_plan.args, config.buildSystemCommand);

    Session session;
    auto session_plan = Session::make_run_plan(std::move(launch_plan),
                                               ServiceImpl::Factory{.runtime = &config.runtime});

    return session.run(std::move(session_plan));
}

int64_t execute_service(ipc::ServiceMode mode, const js::CatterConfig& config) {
    switch(mode) {
        case ipc::ServiceMode::INJECT: {
            return inject(config);
        }
        default: {
            throw std::runtime_error(
                std::format("UnExpected mode: {:0x}", static_cast<uint8_t>(mode)));
        }
    }
    throw std::runtime_error("Not implemented");
}

}  // namespace

void run(const core::Option::CatterOption& opt) {
    auto startup = to_startup_config(opt);
    auto plan = build_runtime_plan(startup);
    auto script_content = load_script_content(plan.script_path);

    js::init_qjs({.pwd = plan.working_dir});
    js::run_js_file(script_content, plan.script_path);

    auto config = js::on_start({
        .scriptPath = plan.script_path,
        .scriptArgs = plan.script_args,
        .buildSystemCommand = plan.build_system_command,
        .runtime = *plan.runtime,
        .options = {.log = plan.log},
        .isScriptSupported = true,
    });

    js::on_finish(js::Tag<js::EventType::finish>{
        .code = execute_service(plan.mode, config),
    });
}

}  // namespace catter::app
