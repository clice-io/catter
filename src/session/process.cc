module;

#include "modules/prelude.h"

#include "support/messages.h"

#ifdef CATTER_WINDOWS
#include <windows.h>

#include "support/exception.h"
#else
#include <fcntl.h>
#include <pthread.h>
#include <sys/resource.h>
#include <unistd.h>
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
    // in the same foreground process group. On Linux it is the kernel's
    // (SI_KERNEL), and a process's has its sender's pid, but 0 for a sender
    // outside this PID namespace, as `docker stop` is. macOS leaves si_code 0
    // for one kill sends: the sender tells them apart.
#ifdef CATTER_LINUX
    const bool sent = info->si_code != SI_KERNEL;
#else
    const bool sent = info->si_pid != 0;
#endif
    if(sent) {
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

/// The standard streams this process was started without, which it holds open
/// on the null device: what it opens would take their numbers otherwise, and
/// reach a program it starts as that stream.
std::array<bool, 3> held = {};

#endif

}  // namespace

void hold_closed_streams() {
#ifndef CATTER_WINDOWS
    for(int fd = 0; fd < 3; fd += 1) {
        if(::fcntl(fd, F_GETFD) < 0) {
            // The lowest number free is `fd`: the streams below it are open.
            held[fd] = ::open("/dev/null", O_RDWR | O_CLOEXEC) == fd;
        }
    }
#endif
}

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

int exit_like(const kota::process::exit_status& status) {
#ifndef CATTER_WINDOWS
    if(status.term_signal != 0) {
        // No core of this process beside the program's own.
        rlimit none{};
        ::setrlimit(RLIMIT_CORE, &none);
        std::signal(status.term_signal, SIG_DFL);
        // The build may have blocked it for the programs it starts.
        sigset_t signals;
        sigemptyset(&signals);
        sigaddset(&signals, status.term_signal);
        ::pthread_sigmask(SIG_UNBLOCK, &signals, nullptr);
        std::raise(status.term_signal);
    }
#endif
    return static_cast<int>(result_of(status).code);
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
#else
    if(run.stand_in) {
        for(int fd = 0; fd < 3; fd += 1) {
            if(held[fd]) {
                opts.streams[fd] = kota::process::stdio::closed();
            }
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

#ifdef CATTER_WINDOWS

OpenedProcess::OpenedProcess(std::int64_t pid) {
    auto opened = kota::process::open(static_cast<int>(pid));
    if(!opened) {
        throw cpptrace::runtime_error(
            std::format("Failed to open process {}: {}", pid, opened.error().message()));
    }
    waiter = std::move(*opened);
    // What injecting into it takes, which waiting for it does not.
    process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_READ |
                              PROCESS_VM_WRITE | PROCESS_TERMINATE | SYNCHRONIZE,
                          FALSE,
                          static_cast<DWORD>(pid));
    if(process == nullptr) {
        throw catter::system_error(GetLastError(),
                                   std::system_category(),
                                   std::format("Failed to open process {}", pid));
    }
}

OpenedProcess::~OpenedProcess() {
    CloseHandle(process);
}

bool OpenedProcess::exited() const noexcept {
    return WaitForSingleObject(process, 0) == WAIT_OBJECT_0;
}

void OpenedProcess::terminate(std::uint32_t code) noexcept {
    TerminateProcess(process, code);
}

kota::task<kota::process::exit_status> OpenedProcess::wait() {
    auto exit = co_await waiter.wait();
    if(!exit) {
        throw cpptrace::runtime_error(
            std::format("Process wait failed `{}`", exit.error().message()));
    }
    co_return *exit;
}

#endif

}  // namespace catter
