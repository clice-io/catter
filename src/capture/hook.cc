module;

#include "modules/prelude.h"

#include "support/messages.h"
#include "support/paths.h"

#ifdef CATTER_WINDOWS
#include "support/log.h"
#include "support/program.h"
#else
#include "support/hook.h"
#endif

module catter;

import :capture.backend;
import :capture.ipc;
import :session.process;
import :session.session;

#ifdef CATTER_WINDOWS
import :capture.inject;
#endif

namespace catter {
namespace {

#ifdef CATTER_WINDOWS
/// Runs the build command as the hook runs each command it starts: decided by
/// the handlers, started with the hook, and its end told to them once the
/// exits of the programs that ended before it are.
kota::task<kota::process::exit_status> run_build(Session& session,
                                                 data::command command,
                                                 ipc::Server& server) {
    const auto id = session.new_id();
    const auto line = std::ranges::to<std::string>(command.args | std::views::join_with(' '));
    auto decision = co_await session.decide(id, std::nullopt, std::move(command));
    if(decision.type != data::action::RUN) {
        const kota::process::exit_status status{.code =
                                                    decision.type == data::action::DROP ? 0 : 1};
        co_await session.exited(id, result_of(status, std::chrono::microseconds{}));
        co_return status;
    }

    std::string error;
    try {
        LOG_INFO("Starting the build command `{}` in {}", line, decision.cmd.cwd);
        const auto start = std::chrono::steady_clock::now();
        const auto status =
            co_await run_process(hook_options(std::move(decision.cmd), id, server.pipe()),
                                 {.stand_in = true});
        const auto result = result_of(status, start);
        co_await server.ended();
        co_await session.exited(id, result);
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
        if(auto library = program_path(hook::LIBRARY);
           library.find_first_of(hook::PRELOAD_SEPARATORS) != std::string::npos) {
            throw cpptrace::runtime_error(std::format(
                "catter cannot preload its hook `{}` into the build: {} splits paths at {}. Install catter in a directory whose path has none",
                library,
                hook::PRELOAD,
                hook::PRELOAD_SEPARATORS_NAMED));
        }
#endif

        ipc::Server server(session);
#ifdef CATTER_WINDOWS
        data::command command{
            .cwd = build.cwd,
            .executable = resolve_program(build.command.front(), build.cwd),
            .args = build.command,
        };
        for(const auto& [name, value]: kota::sys::environment()) {
            command.env.push_back(std::format("{}={}", name, value));
        }
        co_return co_await server.serve([&, command = std::move(command)]() mutable {
            return run_build(session, std::move(command), server);
        });
#else
        kota::process::options proxy{
            .program = program_path(PROXY),
            .args = {"-p", "0", "--ipc", std::string(server.pipe()), "--"},
            .cwd = build.cwd,
        };
        proxy.args.append_range(build.command);
        co_return co_await server.serve([proxy = std::move(proxy)]() mutable {
            return run_process(std::move(proxy), {.stand_in = true});
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
