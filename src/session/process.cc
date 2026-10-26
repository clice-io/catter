module;

#include "modules/prelude.h"

#ifdef CATTER_WINDOWS
#include <windows.h>
#endif

module catter;

import :session.process;

namespace catter {

namespace {

#ifdef CATTER_WINDOWS

/// The console sends Ctrl+C and Ctrl+Break to the program too, which ends as
/// it chooses to.
BOOL WINAPI outlive_interrupt(DWORD event) {
    return event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT;
}

/// While it lives, this process waits out an interrupt for the program it
/// stands in for.
struct StandIn {
    explicit StandIn([[maybe_unused]] int pid) {
        SetConsoleCtrlHandler(outlive_interrupt, TRUE);
    }

    StandIn(const StandIn&) = delete;
    StandIn& operator= (const StandIn&) = delete;

    ~StandIn() {
        SetConsoleCtrlHandler(outlive_interrupt, FALSE);
    }
};

#else

/// The program this process stands in for.
std::atomic<int> stood_in = -1;

void pass_on(int signum, siginfo_t* info, void*) {
    // One the terminal sends, which no process does, reaches the program too,
    // in the same foreground process group. macOS leaves si_code 0 for one
    // kill sends: the sender tells them apart.
    if(info->si_pid != 0) {
        ::kill(stood_in.load(), signum);
    }
}

/// While it lives, the signals that ask this process to stop go on to the
/// program it stands in for, whose exit it waits for.
class StandIn {
public:
    explicit StandIn(int pid) {
        stood_in = pid;
        for(std::size_t i = 0; i < SIGNALS.size(); i += 1) {
            ::sigaction(SIGNALS[i], nullptr, &previous[i]);
            // One the build ignores stays ignored.
            if(previous[i].sa_handler == SIG_IGN) {
                continue;
            }
            struct sigaction action{};
            action.sa_sigaction = pass_on;
            action.sa_flags = SA_SIGINFO | SA_RESTART;
            ::sigaction(SIGNALS[i], &action, nullptr);
        }
    }

    StandIn(const StandIn&) = delete;
    StandIn& operator= (const StandIn&) = delete;

    ~StandIn() {
        for(std::size_t i = 0; i < SIGNALS.size(); i += 1) {
            ::sigaction(SIGNALS[i], &previous[i], nullptr);
        }
    }

private:
    constexpr static std::array SIGNALS = {SIGHUP, SIGINT, SIGTERM};

    std::array<struct sigaction, SIGNALS.size()> previous{};
};

#endif

}  // namespace

kota::process::options process_options(const data::command& command) {
    kota::process::options opts{
        .program = command.executable,
        .env = std::vector<kota::process::env_var>(),
        .cwd = command.cwd,
    };
    for(std::string_view entry: command.env) {
        // Windows names the current directory of each drive `=C:`.
        const auto equals = entry.find('=', 1);
        opts.env->push_back({
            .name = std::string(entry.substr(0, equals)),
            .value = equals == std::string_view::npos ? "" : std::string(entry.substr(equals + 1)),
        });
    }
    if(!command.args.empty()) {
        opts.arg0 = command.args.front();
        opts.args.assign(command.args.begin() + 1, command.args.end());
    }
#ifdef CATTER_WINDOWS
    kota::sys::windows::join_command_line(opts.windows_command_line, command.args);
#endif
    return opts;
}

data::process_result result_of(const kota::process::exit_status& status) {
    return {.code = status.term_signal == 0 ? status.code : 128 + status.term_signal};
}

kota::task<kota::process::exit_status> run_process(kota::process::options opts, RunOptions run) {
#ifndef CATTER_MAC
    // macOS has nothing to end a child with its parent by: `started` sees to
    // it.
    opts.creation.exit_with_parent = run.stand_in;
#endif
    if(run.stand_in) {
        opts.creation.keep_signals = true;
    }
#ifdef CATTER_WINDOWS
    opts.creation.windows_inherit_handles = run.stand_in;
    if(run.stand_in) {
        // The program gets copies of them: a server it starts would keep
        // these too, and with them whatever reads the build's output, open
        // for as long as it lives.
        for(auto which: {STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE}) {
            SetHandleInformation(GetStdHandle(which), HANDLE_FLAG_INHERIT, 0);
        }
    }
#endif
    auto spawned = kota::process::spawn(opts);
    if(!spawned) {
        throw cpptrace::runtime_error(std::format("Process `{}` spawn failed, because `{}`",
                                                  opts.program,
                                                  spawned.error().message()));
    }
    std::optional<StandIn> stand_in;
    if(run.stand_in) {
        stand_in.emplace(spawned->proc.pid());
    }
    if(run.started) {
        run.started(spawned->proc.pid());
    }
    auto exit = co_await spawned->proc.wait();
    if(!exit) {
        throw cpptrace::runtime_error(
            std::format("Process wait failed `{}`", exit.error().message()));
    }
    co_return *exit;
}

}  // namespace catter
