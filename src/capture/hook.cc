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

import :capture.backend;
import :session.ipc;
import :session.process;
import :session.session;

#ifdef CATTER_WINDOWS
import :capture.inject.hook;
import :capture.inject.windows.inject;
#endif

namespace catter {
namespace {

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

/// Serves the hook in a program of the build as it starts a program: the hook
/// asks what the program is to do, starts it suspended and leaves it to
/// catter, which makes it run as decided and waits for its exit.
class Service final : public ipc::Service {
public:
    Service(Session& session, data::ipcid_t id, Programs& programs) :
        ipc::Service(session, id), programs(programs) {}

    kota::task<data::action> make_decision(data::command cmd) noexcept override {
        co_await programs.ended();
        auto decision = co_await ipc::Service::make_decision(std::move(cmd));
        decided = decision.type;
        executable = decision.cmd.executable;
        co_return decision;
    }

    kota::task<bool> started(std::int64_t pid) noexcept override {
        std::string error;
        try {
            program.emplace(pid);
            switch(decided) {
                case data::action::RUN: {
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
            session.fail(std::current_exception());
            co_return;
        }
        co_await finish(result_of(status));
        held.reset();
        program.reset();
    }

private:
    /// Ends the program started, which is not to run, with exit code `code`:
    /// from within, once its caller resumes it, if it can have the hook.
    void end(std::uint32_t code) {
        if(!inject_end(program->handle(), executable, code)) {
            program->terminate(code);
        }
    }

private:
    Programs& programs;

    /// What was decided for the command, and its program.
    decltype(data::action::type) decided = data::action::FAIL;

    std::string executable;

    /// The program the hook started, from its start until its exit is told.
    std::optional<OpenedProcess> program;

    std::optional<Programs::Hold> held;
};

/// Runs the build command as the hook runs each command it starts: decided by
/// the handlers, started with the hook, and its end told to them once the
/// exits of the programs that ended before it are.
kota::task<kota::process::exit_status> run_build(Session& session,
                                                 data::command command,
                                                 Programs& programs) {
    const auto id = session.new_id();
    const auto line = std::ranges::to<std::string>(command.args | std::views::join_with(' '));
    auto decision = co_await session.decide(id, std::nullopt, std::move(command));
    if(decision.type != data::action::RUN) {
        const kota::process::exit_status status{.code =
                                                    decision.type == data::action::DROP ? 0 : 1};
        co_await session.exited(id, result_of(status));
        co_return status;
    }

    std::string error;
    try {
        LOG_INFO("Starting the build command `{}` in {}", line, decision.cmd.cwd);
        const auto status =
            co_await run_process(hook_options(std::move(decision.cmd), id, programs.pipe_name()),
                                 {.stand_in = true});
        co_await programs.ended();
        co_await session.exited(id, result_of(status));
        co_return status;
    } catch(const cpptrace::exception& ex) {
        error = ex.message();
        LOG_ERROR("{}\nStack trace:\n{}", ex.message(), ex.trace().to_string());
    }
    co_await session.uncaptured(id, std::nullopt, std::format("Run `{}` failed:\n{}", line, error));
    co_return kota::process::exit_status{.code = -1};
}
#endif

class HookBackend final : public Backend {
public:
    std::string_view name() const noexcept override {
        return "hook";
    }

    bool follows_decisions() const noexcept override {
        return true;
    }

    kota::task<kota::process::exit_status> run(const Build& build,
                                               Session& session) const override {
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
#ifdef CATTER_WINDOWS
        Programs programs(pipe_name);
        data::command command{
            .cwd = build.cwd,
            .executable = resolve_program(build.command.front(), build.cwd),
            .args = build.command,
        };
        for(const auto& [name, value]: kota::sys::environment()) {
            command.env.push_back(std::format("{}={}", name, value));
        }
        co_return co_await session.serve(
            pipe_name,
            [&, command = std::move(command)]() mutable {
                return run_build(session, std::move(command), programs);
            },
            [&](data::ipcid_t id, kota::pipe&& client) {
                return ipc::accept(std::make_unique<Service>(session, id, programs),
                                   std::move(client));
            });
#else
        kota::process::options proxy{
            .program = program_path(config::PROXY_EXE_NAME),
            .args = {"-p", "0", "--ipc", pipe_name, "--"},
            .cwd = build.cwd,
        };
        proxy.args.append_range(build.command);
        co_return co_await session.serve(
            pipe_name,
            [proxy = std::move(proxy)]() mutable {
                return run_process(std::move(proxy), {.stand_in = true});
            },
            [&](data::ipcid_t id, kota::pipe&& client) {
                return ipc::accept(std::make_unique<ipc::Service>(session, id), std::move(client));
            });
#endif
    }
};

}  // namespace

const Backend& hook_backend() noexcept {
    const static HookBackend backend;
    return backend;
}

}  // namespace catter
