#include "runtime_driver.h"

#include <array>
#include <filesystem>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <cpptrace/exceptions.hpp>

#include "ipc.h"
#include "replay.h"
#include "session.h"
#include "config/catter-proxy.h"
#include "config/ipc.h"
#include "util/crossplat.h"
#include "util/encoding.h"

namespace catter::core {
namespace {

class InjectService final : public ipc::InjectService {
public:
    InjectService(data::ipcid_t id, Handler& handler) : id(id), handler(handler) {}

    kota::task<data::ipcid_t> create(data::ipcid_t parent_id) noexcept override {
        this->parent_id = parent_id;
        co_return this->id;
    }

    kota::task<data::action> make_decision(data::command cmd) noexcept override {
        auto act = co_await handler.on_command(this->id, this->parent_id, cmd);

        switch(act.type) {
            case Action::Type::Drop: {
                co_return data::action{.type = data::action::DROP, .cmd = {}};
            }
            case Action::Type::Skip: {
                co_return data::action{.type = data::action::INJECT, .cmd = std::move(cmd)};
            }
            case Action::Type::Modify: {
                co_return data::action{.type = data::action::INJECT, .cmd = std::move(act.command)};
            }
            // TODO: handle Action::Type::Abort
            case Action::Type::Abort: {
                throw cpptrace::runtime_error("Unhandled action type");
            }
        }
        std::unreachable();
    }

    kota::task<> finish(data::process_result result) noexcept override {
        co_await handler.on_exit(this->id, std::move(result));
    }

    kota::task<> report_error(data::ipcid_t parent_id, std::string error_msg) noexcept override {
        co_await handler.on_error(this->id, parent_id, std::move(error_msg));
    }

    struct Factory {
        Handler* handler;

        std::unique_ptr<InjectService> operator() (data::ipcid_t id) const {
            return std::make_unique<InjectService>(id, *handler);
        }
    };

private:
    data::ipcid_t id = 0;

    data::ipcid_t parent_id = 0;

    Handler& handler;
};

class InjectRuntimeDriver final : public RuntimeDriver {
public:
    std::string_view name() const noexcept override {
        return "inject";
    }

    kota::task<data::process_result> execute(const Build& build, Handler& handler) const override {
        if(build.command.empty()) {
            throw cpptrace::runtime_error("buildSystemCommand must not be empty");
        }

        auto proxy_path = util::get_catter_root_path() / config::proxy::EXE_NAME;
        auto pipe_name = config::ipc::session_pipe_name();
        Session::ProcessLaunchPlan launch_plan{
            .cwd = build.cwd,
            .executable = util::to_utf8(proxy_path),
            .args =
                {
                       util::to_utf8(proxy_path),
                       "-p", "0",
                       "--ipc", pipe_name,
                       "--", },
            .mode = build.stdio_mode,
        };
        util::append_range_to_vector(launch_plan.args, build.command);

        Session session;
        auto session_plan = Session::make_run_plan(std::move(launch_plan),
                                                   std::move(pipe_name),
                                                   InjectService::Factory{.handler = &handler});

        co_return co_await session.run(std::move(session_plan));
    }
};

const InjectRuntimeDriver* inject_runtime_driver() noexcept {
    const static InjectRuntimeDriver driver;
    return &driver;
}

/// Plays a recorded build (see replay.h) instead of running one: the build
/// command is the replay file.
class ReplayRuntimeDriver final : public RuntimeDriver {
public:
    std::string_view name() const noexcept override {
        return "replay";
    }

    kota::task<data::process_result> execute(const Build& build, Handler& handler) const override {
        if(build.command.size() != 1) {
            throw cpptrace::runtime_error(
                "the replay mode takes a replay file for the build command: " "catter -m replay <script> -- <file>");
        }
        auto file = util::from_utf8(build.cwd) / util::from_utf8(build.command.front());
        co_return co_await replay_build(file.lexically_normal(), handler);
    }
};

const ReplayRuntimeDriver* replay_runtime_driver() noexcept {
    const static ReplayRuntimeDriver driver;
    return &driver;
}

auto runtime_drivers() noexcept {
    return std::array<const RuntimeDriver*, 2>{inject_runtime_driver(), replay_runtime_driver()};
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

const RuntimeDriver* default_runtime_driver() noexcept {
    return inject_runtime_driver();
}

}  // namespace catter::core
