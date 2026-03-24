#include "app_runner.h"

#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "app_config.h"
#include "ipc.h"
#include "js.h"
#include "qjs.h"
#include "session.h"
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
                    }};
            }
            default: throw std::runtime_error("Unhandled action type");
        }
    }

    void finish(int64_t code) override {
        js::on_execution(this->id, js::Tag<js::EventType::finish>{.code = code});
    }

    void report_error(data::ipcid_t parent_id, std::string error_msg) override {
        js::on_command(id, std::unexpected(js::CatterErr{.msg = std::move(error_msg)}));
    }

    struct Factory {
        const js::CatterRuntime* runtime;

        std::unique_ptr<ServiceImpl> operator() (data::ipcid_t id) {
            return std::make_unique<ServiceImpl>(id, runtime);
        }
    };

private:
    data::ipcid_t id = 0;
    data::ipcid_t parent_id = 0;
    const js::CatterRuntime* runtime = nullptr;
};

void inject(const RuntimePlan& plan) {
    auto script_content = load_script_content(plan.script_path);
    js::run_js_file(script_content, plan.script_path);

    auto new_config = js::on_start({
        .scriptPath = plan.script_path,
        .scriptArgs = plan.script_args,
        .buildSystemCommand = plan.build_system_command,
        .runtime = *plan.runtime,
        .options = {.log = plan.log},
        .isScriptSupported = true,
    });

    Session session;
    auto ret = session.run(new_config.buildSystemCommand, ServiceImpl::Factory{.runtime = plan.runtime});

    js::on_finish(js::Tag<js::EventType::finish>{
        .code = ret,
    });
}

}  // namespace

void run(const core::Option::CatterOption& opt) {
    auto startup = to_startup_config(opt);
    auto plan = build_runtime_plan(startup);

    js::init_qjs({.pwd = plan.working_dir});

    switch(plan.mode) {
        case ipc::ServiceMode::INJECT: {
            inject(plan);
            break;
        }
        default: {
            throw std::runtime_error(std::format("UnExpected mode: {:0x}", static_cast<uint8_t>(plan.mode)));
        }
    }
}

}  // namespace catter::app
