module;

#include "modules/prelude.h"

#include "support/messages.h"

#ifdef CATTER_WINDOWS
#include <windows.h>
#else
#include <fcntl.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sys/wait.h>
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

/// The standard streams this process was started without, which it holds open
/// on the null device: what it opens would take their numbers otherwise, and
/// reach a program it starts as that stream.
std::array<bool, 3> held = {};

/// The descriptors past the standard streams this process was started with,
/// which the program it stands in for gets.
std::vector<int> inherited;

/// Lets go of the descriptors the program this process stands in for has
/// from it: those it was started with past the standard streams, and with
/// `streams` the standard streams, onto the null device.
void release_descriptors(bool streams) {
    for(int fd: inherited) {
        ::close(fd);
    }
    inherited.clear();
    if(!streams) {
        return;
    }
    const int null = ::open("/dev/null", O_RDWR | O_CLOEXEC);
    for(int fd = 0; fd < 3; fd += 1) {
        if(!held[fd]) {
            ::dup2(null, fd);
        }
    }
    ::close(null);
}

#endif

/// How a program ended, as a shell reports it: a signal that ended it as 128
/// and the signal's number.
std::int64_t code_of(const kota::process::exit_status& status) {
    return status.term_signal == 0 ? status.code : 128 + status.term_signal;
}

}  // namespace

#ifdef CATTER_WINDOWS

StandIn::StandIn([[maybe_unused]] int pid, [[maybe_unused]] bool release_streams) {
    SetConsoleCtrlHandler(outlive_interrupt, TRUE);
}

StandIn::~StandIn() {
    SetConsoleCtrlHandler(outlive_interrupt, FALSE);
}

#else

StandIn::StandIn(int pid, bool release_streams) {
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
    release_descriptors(release_streams);
}

StandIn::~StandIn() {
    for(std::size_t i = 0; i < SIGNALS.size(); i += 1) {
        ::sigaction(SIGNALS[i], &previous[i], nullptr);
    }
}

#endif

void prepare_descriptors() {
#ifndef CATTER_WINDOWS
    for(int fd = 0; fd < 3; fd += 1) {
        if(::fcntl(fd, F_GETFD) < 0) {
            // The lowest number free is `fd`: the streams below it are open.
            held[fd] = ::open("/dev/null", O_RDWR | O_CLOEXEC) == fd;
        }
    }
#ifdef CATTER_LINUX
    constexpr std::string_view descriptors = "/proc/self/fd";
#else
    constexpr std::string_view descriptors = "/dev/fd";
#endif
    auto directory = kota::fs::Directory::open(descriptors);
    if(!directory) {
        return;
    }
    for(const auto& entry: *directory) {
        if(!entry.has_value()) {
            break;
        }
        int fd = -1;
        const auto& name = entry->name;
        // The directory's own descriptor closes on exec: what exec left open
        // does not.
        if(std::from_chars(name.data(), name.data() + name.size(), fd).ec == std::errc() &&
           fd > 2 && ::fcntl(fd, F_GETFD) == 0) {
            inherited.push_back(fd);
        }
    }
#endif
}

std::vector<std::string> environment() {
    std::vector<std::string> env;
    for(const auto& [name, value]: kota::sys::environment()) {
        env.push_back(std::format("{}={}", name, value));
    }
    return env;
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

data::process_result result_of(const kota::process::exit_status& status,
                               std::chrono::microseconds ran) {
    return {.code = code_of(status), .duration = ran.count()};
}

data::process_result result_of(const kota::process::exit_status& status,
                               std::chrono::steady_clock::time_point started) {
    return result_of(status,
                     std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::steady_clock::now() - started));
}

#ifndef CATTER_WINDOWS
kota::process::exit_status exit_status_of(int status) {
    return {
        .code = WIFEXITED(status) ? WEXITSTATUS(status) : 0,
        .term_signal = WIFSIGNALED(status) ? WTERMSIG(status) : 0,
    };
}
#endif

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
    return static_cast<int>(code_of(status));
}

kota::process::options standing_in(kota::process::options opts) {
#ifndef CATTER_MAC
    // macOS has nothing to end a child with its parent by: RunOptions::started
    // sees to it.
    opts.creation.exit_with_parent = true;
#endif
    opts.creation.keep_signals = true;
#ifdef CATTER_WINDOWS
    opts.creation.windows_inherit_handles = true;
    // The program gets copies of them: a server it starts would keep these
    // too, and with them whatever reads the build's output, open for as long
    // as it lives.
    for(auto which: {STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE}) {
        SetHandleInformation(GetStdHandle(which), HANDLE_FLAG_INHERIT, 0);
    }
#else
    for(int fd = 0; fd < 3; fd += 1) {
        if(held[fd]) {
            opts.streams[fd] = kota::process::stdio::closed();
        }
    }
#endif
    return opts;
}

kota::task<kota::process::exit_status> run_process(kota::process::options opts, RunOptions run) {
    if(run.stand_in) {
        opts = standing_in(std::move(opts));
    }
    auto spawned = kota::process::spawn(opts);
    if(!spawned) {
        throw cpptrace::runtime_error(std::format("Process `{}` spawn failed, because `{}`",
                                                  opts.program,
                                                  spawned.error().message()));
    }
    std::optional<StandIn> stand_in;
    if(run.stand_in) {
        stand_in.emplace(spawned->proc.pid(), run.release_streams);
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
