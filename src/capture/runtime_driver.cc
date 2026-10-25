#include "capture/runtime_driver.h"

#include <array>
#include <exception>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <cpptrace/exceptions.hpp>

#include "capture/replay.h"
#include "session/ipc.h"
#include "session/session.h"
#include "support/config/catter-proxy.h"
#include "support/config/ipc.h"
#include "support/crossplat.h"
#include "support/encoding.h"

namespace catter {
namespace {

/// The error that ends a session: the first one a handler threw. The only
/// way to set it is `set`, which keeps the first.
class Failure {
public:
    explicit operator bool() const noexcept {
        return error != nullptr;
    }

    void set(std::exception_ptr error) noexcept {
        if(!this->error) {
            this->error = std::move(error);
        }
    }

    void rethrow() const {
        if(error) {
            std::rethrow_exception(error);
        }
    }

private:
    std::exception_ptr error;
};

/// The build command's proxy is started with parent 0: no command started it.
std::optional<data::ipcid_t> parent_of(data::ipcid_t parent_id) {
    if(parent_id == 0) {
        return std::nullopt;
    }
    return parent_id;
}

class InjectService final : public ipc::InjectService {
public:
    InjectService(data::ipcid_t id, Handler& handler, Failure& failure) :
        id(id), handler(handler), failure(failure) {}

    kota::task<data::ipcid_t> create(data::ipcid_t parent_id) noexcept override {
        this->parent_id = parent_id;
        co_return this->id;
    }

    kota::task<data::action> make_decision(data::command cmd) noexcept override {
        if(failure) {
            co_return data::action{.type = data::action::FAIL};
        }
        Action act;
        try {
            act = co_await handler.on_command(this->id, parent_of(this->parent_id), cmd);
        } catch(...) {
            failure.set(std::current_exception());
            co_return data::action{.type = data::action::FAIL};
        }

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
        }
        std::unreachable();
    }

    kota::task<> finish(data::process_result result) noexcept override {
        if(failure) {
            co_return;
        }
        try {
            co_await handler.on_exit(this->id, std::move(result));
        } catch(...) {
            failure.set(std::current_exception());
        }
    }

    kota::task<> report_error(data::ipcid_t parent_id, std::string error_msg) noexcept override {
        if(failure) {
            co_return;
        }
        try {
            co_await handler.on_error(this->id, parent_of(parent_id), std::move(error_msg));
        } catch(...) {
            failure.set(std::current_exception());
        }
    }

    struct Factory {
        Handler* handler;

        Failure* failure;

        std::unique_ptr<InjectService> operator() (data::ipcid_t id) const {
            return std::make_unique<InjectService>(id, *handler, *failure);
        }
    };

private:
    data::ipcid_t id = 0;

    data::ipcid_t parent_id = 0;

    Handler& handler;

    Failure& failure;
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

        auto proxy_path = get_catter_root_path() / config::PROXY_EXE_NAME;
        auto pipe_name = config::session_pipe_name();
        Session::ProcessLaunchPlan launch_plan{
            .cwd = build.cwd,
            .executable = to_utf8(proxy_path),
            .args =
                {
                       to_utf8(proxy_path),
                       "-p", "0",
                       "--ipc", pipe_name,
                       "--", },
        };
        append_range_to_vector(launch_plan.args, build.command);

        Failure failure;
        Session session;
        auto session_plan = Session::make_run_plan(
            std::move(launch_plan),
            std::move(pipe_name),
            InjectService::Factory{.handler = &handler, .failure = &failure});

        auto result = co_await session.run(std::move(session_plan));
        failure.rethrow();
        co_return result;
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
                "the replay mode takes a replay file for the build command: " "catter -m replay [--handler ...] [built-in] -- <file>");
        }
        auto file = from_utf8(build.cwd) / from_utf8(build.command.front());
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

}  // namespace catter
