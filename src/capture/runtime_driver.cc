module;

#include "modules/prelude.h"

#include "support/config/catter-proxy.h"
#include "support/config/ipc.h"
#include "support/paths.h"

module catter;

import :capture.replay;
import :capture.runtime_driver;
import :session.ipc;
import :session.session;

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

    kota::task<kota::process::exit_status> execute(const Build& build,
                                                   Handler& handler) const override {
        assert(!build.command.empty());

        auto pipe_name = config::session_pipe_name();
        Session::ProcessLaunchPlan launch_plan{
            .cwd = build.cwd,
            .executable = program_path(config::PROXY_EXE_NAME),
            .args = {"-p", "0", "--ipc", pipe_name, "--"},
        };
        launch_plan.args.insert(launch_plan.args.end(), build.command.begin(), build.command.end());

        Failure failure;
        Session session;
        auto session_plan = Session::make_run_plan(
            std::move(launch_plan),
            std::move(pipe_name),
            InjectService::Factory{.handler = &handler, .failure = &failure});

        auto status = co_await session.run(std::move(session_plan));
        failure.rethrow();
        co_return status;
    }
};

/// Plays a recorded build (see replay.cppm) instead of running one: the build
/// command is the replay file.
class ReplayRuntimeDriver final : public RuntimeDriver {
public:
    std::string_view name() const noexcept override {
        return "replay";
    }

    kota::task<kota::process::exit_status> execute(const Build& build,
                                                   Handler& handler) const override {
        assert(build.command.size() == 1);
        auto file = build.cwd;
        kota::fs::path::push(file, build.command.front());
        kota::fs::path::normalize(file);
        co_return co_await replay_build(file, handler);
    }
};

}  // namespace

const RuntimeDriver& inject_runtime_driver() noexcept {
    const static InjectRuntimeDriver driver;
    return driver;
}

const RuntimeDriver& replay_runtime_driver() noexcept {
    const static ReplayRuntimeDriver driver;
    return driver;
}

}  // namespace catter
