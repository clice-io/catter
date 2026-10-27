module;

#include "modules/prelude.h"

#include "support/config/ipc.h"
#include "support/messages.h"
#include "support/paths.h"
#include "support/program.h"

#ifdef CATTER_WINDOWS
#include "support/log.h"
#else
#include "support/config/catter-proxy.h"
#include "support/config/hook_unix.h"
#endif

module catter;

import :capture.replay;
import :capture.runtime_driver;
import :session.ipc;
import :session.process;
import :session.session;

#ifdef CATTER_WINDOWS
import :capture.inject.hook;
import :capture.inject.windows.inject;
#endif

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

/// The build command is started with parent 0: no command started it.
std::optional<data::ipcid_t> parent_of(data::ipcid_t parent_id) {
    if(parent_id == 0) {
        return std::nullopt;
    }
    return parent_id;
}

#ifdef CATTER_WINDOWS
/// The programs the hook starts: the pipe the hook in each reports to, and
/// those whose exits are not told yet. The build sees a program end before
/// catter does: the commands it starts after, and its own end, wait for the
/// exits of the programs that have ended to be told first.
class Programs {
public:
    /// A program whose exit is not told yet.
    struct Untold {
        const OpenedProcess* program;

        /// Set once its exit is told: shared with those waiting for it.
        std::shared_ptr<kota::event> told;
    };

    explicit Programs(std::string_view pipe_name) : pipe(pipe_name) {}

    /// The pipe the hook in each program reports to.
    std::string_view pipe_name() const noexcept {
        return pipe;
    }

    /// Holds `program` among those whose exit is not told yet, until it goes.
    class Hold {
    public:
        Hold(Programs& programs, const OpenedProcess& program) :
            programs(programs),
            entry(programs.untold.insert(
                programs.untold.end(),
                {.program = &program, .told = std::make_shared<kota::event>()})) {}

        Hold(const Hold&) = delete;
        Hold& operator= (const Hold&) = delete;

        ~Hold() {
            entry->told->set();
            programs.untold.erase(entry);
        }

    private:
        Programs& programs;

        std::list<Untold>::iterator entry;
    };

    /// Waits until the exit of each program held that has ended is told.
    kota::task<> ended() {
        std::vector<std::shared_ptr<kota::event>> ended;
        for(const auto& entry: untold) {
            if(entry.program->exited()) {
                ended.push_back(entry.told);
            }
        }
        for(const auto& told: ended) {
            co_await told->wait();
        }
    }

private:
    std::string_view pipe;

    std::list<Untold> untold;
};
#endif

class InjectService final : public ipc::InjectService {
public:
#ifdef CATTER_WINDOWS
    InjectService(data::ipcid_t id, Handler& handler, Failure& failure, Programs& programs) :
        programs(programs), id(id), handler(handler), failure(failure) {}
#else
    InjectService(data::ipcid_t id, Handler& handler, Failure& failure) :
        id(id), handler(handler), failure(failure) {}
#endif

    kota::task<data::ipcid_t> create(data::ipcid_t parent_id) noexcept override {
        this->parent_id = parent_id;
        co_return this->id;
    }

    kota::task<data::action> make_decision(data::command cmd) noexcept override {
#ifdef CATTER_WINDOWS
        co_await programs.ended();
        auto decision = co_await decide(std::move(cmd));
        decided = decision.type;
        executable = decision.cmd.executable;
        co_return decision;
#else
        co_return co_await decide(std::move(cmd));
#endif
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

#ifdef CATTER_WINDOWS
    kota::task<bool> started(std::int64_t pid) noexcept override {
        std::string error;
        try {
            program.emplace(pid);
            switch(decided) {
                case data::action::INJECT: {
                    inject_hook(program->handle(), executable, id, programs.pipe_name());
                    break;
                }
                case data::action::DROP: {
                    end(0);
                    break;
                }
                case data::action::FAIL: {
                    end(1);
                    break;
                }
            }
            held.emplace(programs, *program);
            co_return true;
        } catch(const cpptrace::exception& ex) {
            error = ex.message();
            LOG_ERROR("{}\nStack trace:\n{}", ex.message(), ex.trace().to_string());
        }
        program.reset();
        co_await report_error(
            parent_id,
            std::format("`{}` cannot start with the hook: {}", executable, error));
        co_return false;
    }

    kota::task<> wait_started() noexcept override {
        if(!program) {
            co_return;
        }
        kota::process::exit_status status;
        try {
            status = co_await program->wait();
        } catch(...) {
            failure.set(std::current_exception());
            co_return;
        }
        co_await finish(result_of(status));
        held.reset();
        program.reset();
    }
#endif

    struct Factory {
        Handler* handler;

        Failure* failure;

#ifdef CATTER_WINDOWS
        Programs* programs;
#endif

        std::unique_ptr<InjectService> operator() (data::ipcid_t id) const {
#ifdef CATTER_WINDOWS
            return std::make_unique<InjectService>(id, *handler, *failure, *programs);
#else
            return std::make_unique<InjectService>(id, *handler, *failure);
#endif
        }
    };

private:
    /// What the handlers decide for `cmd`: it starts as it is unless they
    /// change it, and when it is not to run its program is told the command
    /// as it was sent, which the hook starts to end at once.
    kota::task<data::action> decide(data::command cmd) {
        if(failure) {
            co_return data::action{.type = data::action::FAIL, .cmd = std::move(cmd)};
        }
        Action act;
        try {
            act = co_await handler.on_command(this->id, parent_of(this->parent_id), cmd);
        } catch(...) {
            failure.set(std::current_exception());
            co_return data::action{.type = data::action::FAIL, .cmd = std::move(cmd)};
        }

        switch(act.type) {
            case Action::Type::Drop: {
                co_return data::action{.type = data::action::DROP, .cmd = std::move(cmd)};
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

#ifdef CATTER_WINDOWS
    /// Ends the program started, which is not to run, with exit code `code`:
    /// from within, once its caller resumes it, if it can have the hook.
    void end(std::uint32_t code) {
        if(!inject_end(program->handle(), executable, code)) {
            program->terminate(code);
        }
    }
#endif

private:
#ifdef CATTER_WINDOWS
    Programs& programs;

    /// What was decided for the command, and its program.
    decltype(data::action::type) decided = data::action::FAIL;

    std::string executable;

    /// The program the hook started, from its start until its exit is told.
    std::optional<OpenedProcess> program;

    std::optional<Programs::Hold> held;
#endif

    data::ipcid_t id = 0;

    data::ipcid_t parent_id = 0;

    Handler& handler;

    Failure& failure;
};

#ifdef CATTER_WINDOWS
/// Runs the build command as the hook runs each command it starts: decided by
/// the handlers, started with the hook, and its end told to them once the
/// exits of the programs that ended before it are.
kota::task<kota::process::exit_status> run_build(data::command command,
                                                 std::unique_ptr<ipc::InjectService> service,
                                                 Programs& programs) {
    const auto id = co_await service->create(0);
    const auto line = std::ranges::to<std::string>(command.args | std::views::join_with(' '));
    auto decision = co_await service->make_decision(std::move(command));
    if(decision.type != data::action::INJECT) {
        const kota::process::exit_status status{.code =
                                                    decision.type == data::action::DROP ? 0 : 1};
        co_await service->finish(result_of(status));
        co_return status;
    }

    std::string error;
    try {
        LOG_INFO("Starting the build command `{}` in {}", line, decision.cmd.cwd);
        const auto status =
            co_await run_process(hook_options(std::move(decision.cmd), id, programs.pipe_name()),
                                 {.stand_in = true});
        co_await programs.ended();
        co_await service->finish(result_of(status));
        co_return status;
    } catch(const cpptrace::exception& ex) {
        error = ex.message();
        LOG_ERROR("{}\nStack trace:\n{}", ex.message(), ex.trace().to_string());
    }
    co_await service->report_error(0, std::format("Run `{}` failed:\n{}", line, error));
    co_return kota::process::exit_status{.code = -1};
}
#endif

class InjectRuntimeDriver final : public RuntimeDriver {
public:
    std::string_view name() const noexcept override {
        return "inject";
    }

    kota::task<kota::process::exit_status> execute(const Build& build,
                                                   Handler& handler) const override {
        assert(!build.command.empty());
#ifndef CATTER_WINDOWS
        if(auto hook = program_path(config::RELATIVE_PATH_OF_HOOK_LIB);
           hook.find_first_of(config::PRELOAD_SEPARATORS) != std::string::npos) {
            throw cpptrace::runtime_error(std::format(
                "catter cannot preload its hook `{}` into the build: {} splits paths at {}. Install catter in a directory whose path has none",
                hook,
                config::KEY_PRELOAD,
                config::PRELOAD_SEPARATORS_NAMED));
        }
#endif

        auto pipe_name = config::session_pipe_name();
        Failure failure;
        Session session;
#ifdef CATTER_WINDOWS
        Programs programs(pipe_name);
        InjectService::Factory factory{.handler = &handler,
                                       .failure = &failure,
                                       .programs = &programs};
        data::command command{
            .cwd = build.cwd,
            .executable = resolve_program(build.command.front(), build.cwd),
            .args = build.command,
        };
        for(const auto& [name, value]: kota::sys::environment()) {
            command.env.push_back(std::format("{}={}", name, value));
        }
        Session::Start start = [&, command = std::move(command)]() mutable {
            return run_build(std::move(command), factory(session.new_id()), programs);
        };
#else
        InjectService::Factory factory{.handler = &handler, .failure = &failure};
        kota::process::options proxy{
            .program = program_path(config::PROXY_EXE_NAME),
            .args = {"-p", "0", "--ipc", pipe_name, "--"},
            .cwd = build.cwd,
        };
        proxy.args.append_range(build.command);
        Session::Start start = [proxy = std::move(proxy)]() mutable {
            return run_process(std::move(proxy), {.stand_in = true});
        };
#endif

        auto status =
            co_await session.run(Session::make_run_plan(std::move(start), pipe_name, factory));
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
