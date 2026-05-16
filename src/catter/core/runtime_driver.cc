#include "runtime_driver.h"

#include <array>
#include <cstdint>
#include <expected>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <cpptrace/exceptions.hpp>

#include "ipc.h"
#include "session.h"
#include "config/catter-proxy.h"
#include "js/js.h"
#include "util/crossplat.h"

namespace catter::core {
namespace {
data::output_mode to_process_output_mode(js::CatterOptions::OutputMode output_mode) {
    switch(output_mode) {
        case js::CatterOptions::OutputMode::inherit: return data::output_mode::inherit;
        case js::CatterOptions::OutputMode::capture: return data::output_mode::capture;
    }

    throw cpptrace::runtime_error("Unhandled catter output mode");
}

class InjectService final : public ipc::InjectService {
public:
    InjectService(data::ipcid_t id, const js::CatterRuntime* runtime) : id(id), runtime(runtime) {}

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
                co_return data::action{.type = data::action::INJECT, .cmd = std::move(cmd)};
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
        co_await js::on_execution(this->id, to_js_process_result(std::move(result)));
        co_return;
    }

    kota::task<> report_error(data::ipcid_t parent_id, std::string error_msg) override {
        (void)parent_id;
        co_await js::on_command(id, std::unexpected(js::CatterErr{.msg = std::move(error_msg)}));
        co_return;
    }

    struct Factory {
        const js::CatterRuntime* runtime;

        std::unique_ptr<InjectService> operator() (data::ipcid_t id) const {
            return std::make_unique<InjectService>(id, runtime);
        }
    };

private:
    data::ipcid_t id = 0;
    data::ipcid_t parent_id = 0;
    const js::CatterRuntime* runtime = nullptr;
};

class InjectRuntimeDriver final : public RuntimeDriver {
public:
    std::string_view name() const noexcept override {
        return "inject";
    }

    const js::CatterRuntime& runtime() const noexcept override {
        const static js::CatterRuntime value{
            .supportActions = {js::ActionType::drop, js::ActionType::skip, js::ActionType::modify},
            .type = js::CatterRuntime::Type::inject,
            .supportParentId = true,
        };
        return value;
    }

    kota::task<data::process_result> execute(const js::CatterConfig& config) const override {
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
            .output_mode = to_process_output_mode(
                config.options.output.value_or(js::CatterOptions::OutputMode::inherit)),
        };
        util::append_range_to_vector(launch_plan.args, config.buildSystemCommand);

        Session session;
        auto session_plan =
            Session::make_run_plan(std::move(launch_plan),
                                   InjectService::Factory{.runtime = &config.runtime});

        co_return co_await session.run(std::move(session_plan));
    }
};

const InjectRuntimeDriver& inject_runtime_driver() noexcept {
    const static InjectRuntimeDriver driver;
    return driver;
}

auto runtime_drivers() noexcept {
    return std::array<const RuntimeDriver*, 1>{&inject_runtime_driver()};
}

}  // namespace

const RuntimeDriver* find_runtime_driver(std::string_view name) noexcept {
    for(const auto* driver: runtime_drivers()) {
        if(driver->name() == name) {
            return driver;
        }
    }
    return nullptr;
}

const RuntimeDriver& default_runtime_driver() noexcept {
    return inject_runtime_driver();
}

js::ProcessResult to_js_process_result(data::process_result result) {
    return js::ProcessResult{
        .code = result.code,
        .stdOut = std::move(result.std_out),
        .stdErr = std::move(result.std_err),
    };
}

}  // namespace catter::core
