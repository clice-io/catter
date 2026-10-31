module;

#include "support/log.h"
#include "support/messages.h"
#include "support/output.h"
#include "support/paths.h"
#include "support/prelude.h"
#include "support/program.h"

#ifdef CATTER_LINUX
#include <fcntl.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/xattr.h>
#include <unistd.h>
#endif

module catter;

import :capture.backend;
import :capture.processes;
import :session.handler;
import :session.process;
import :session.session;

namespace catter {

#ifdef CATTER_LINUX

namespace {

/// PTRACE_EVENT_STOP, which glibc 2.17's <sys/ptrace.h> lacks.
constexpr int EVENT_STOP = 128;

constexpr std::intptr_t OPTIONS =
    PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK | PTRACE_O_TRACECLONE | PTRACE_O_TRACEEXEC;

/// What the tracer saw a process of the build do, each process by its pid.
struct Forked {
    int parent;
    int child;
};

struct Execed {
    int pid;

    data::command command;

    /// Why catter cannot capture the command, when it cannot.
    std::optional<std::string> error;

    /// Whether the program's file is set-user-ID or set-group-ID or has
    /// capabilities, which it runs without as catter traces it.
    bool unprivileged = false;

    chrono::microseconds time;
};

struct Exited {
    int pid;

    async::process::exit_status status;

    chrono::microseconds time;
};

/// The tracer stopped: the build command's process ended as `status` says, or
/// `error` says why catter could not trace it.
struct Done {
    async::process::exit_status status;

    std::optional<std::string> error;
};

using Event = std::variant<Forked, Execed, Exited, Done>;

/// The strings of a /proc file that ends each with a null.
std::vector<std::string> strings_of(std::string_view bytes) {
    if(bytes.ends_with('\0')) {
        bytes.remove_suffix(1);
    }
    if(bytes.empty()) {
        return {};
    }
    return bytes | views::split('\0') |
           views::transform([](auto string) { return ranges::to<std::string>(string); }) |
           ranges::to<std::vector>();
}

struct Lineage {
    /// The process a thread is of: its main thread's ID.
    int process = 0;

    /// That process's parent.
    int parent = 0;
};

/// /proc/<tid>/status of thread `tid`, empty once it has gone.
std::string status_of(int tid) {
    std::string status;
    [[maybe_unused]] auto error = fs::read_file(status, std::format("/proc/{}/status", tid));
    return status;
}

/// The process ID `status`, a /proc/<tid>/status, gives as `name`: 0 for none.
int id_in(std::string_view status, std::string_view name) {
    for(auto line: status | views::split('\n')) {
        std::string_view text(line);
        if(text.starts_with(name) && text.substr(name.size()).starts_with(':')) {
            auto number = text.substr(text.find_first_not_of(" \t", name.size() + 1));
            int id = 0;
            std::from_chars(number.data(), number.data() + number.size(), id);
            return id;
        }
    }
    return 0;
}

/// What /proc/<tid>/status tells of thread `tid`.
Lineage lineage_of(int tid) {
    const auto status = status_of(tid);
    if(status.empty()) {
        return {.process = tid};
    }
    return {.process = id_in(status, "Tgid"), .parent = id_in(status, "PPid")};
}

/// Where the strings of the environment of process `pid` end, as field 51 of
/// `stat`, its /proc/<pid>/stat, tells: execve puts the path it was given
/// (AT_EXECFN) right after them.
std::uintptr_t environment_end(std::string_view stat) {
    // The fields after the program's name, which may hold spaces, start with
    // the third.
    auto fields = stat.substr(stat.rfind(')') + 2) | views::split(' ') | views::drop(51 - 3);
    std::uintptr_t end = 0;
    if(auto field = fields.begin(); field != fields.end()) {
        std::string_view text(*field);
        std::from_chars(text.data(), text.data() + text.size(), end);
    }
    return end;
}

/// The string at `address` in the memory of process `pid`, which this thread
/// traces. A word at a time: Yama keeps a process the build reparented, as a
/// daemon is, from process_vm_readv, but not from the PTRACE_PEEKDATA of its
/// tracer.
std::expected<std::string, std::error_code> string_at(int pid, std::uintptr_t address) {
    std::string string;
    while(true) {
        errno = 0;
        const long word = ::ptrace(PTRACE_PEEKDATA,
                                   pid,
                                   reinterpret_cast<void*>(address + string.size()),
                                   nullptr);
        if(errno != 0) {
            return std::unexpected(std::error_code(errno, std::generic_category()));
        }
        const auto bytes = std::bit_cast<std::array<char, sizeof(word)>>(word);
        const auto end = ranges::find(bytes, '\0');
        string.append(bytes.begin(), end);
        if(end != bytes.end()) {
            return string;
        }
    }
}

void resume(int tid, int signal = 0) {
    ::ptrace(PTRACE_CONT,
             tid,
             nullptr,
             reinterpret_cast<void*>(static_cast<std::intptr_t>(signal)));
}

bool is_stop_signal(int signal) {
    return signal == SIGSTOP || signal == SIGTSTP || signal == SIGTTIN || signal == SIGTTOU;
}

/// Why ptrace refused catter process `pid`, the build command's, as `error`
/// says.
std::string denied(int pid, int error) {
    std::string scope;
    [[maybe_unused]] auto unread = fs::read_file(scope, "/proc/sys/kernel/yama/ptrace_scope");
    std::string why;
    if(const int tracer = id_in(status_of(pid), "TracerPid"); tracer != 0) {
        why = std::format(
            "process {} traces catter's children already, as strace -f or a debugger does, and a process has one tracer",
            tracer);
    } else if(scope.starts_with('2')) {
        why =
            "kernel.yama.ptrace_scope is 2, which lets only a process with CAP_SYS_PTRACE trace another";
    } else if(scope.starts_with('3')) {
        why = "kernel.yama.ptrace_scope is 3, which lets no process trace another";
    } else {
        why = "the system denies ptrace, as a container's seccomp profile or a security module may";
    }
    return std::format(
        "catter cannot trace the build with ptrace ({}): {}. Capture it with --capture hook instead",
        std::error_code(error, std::generic_category()).message(),
        why);
}

/// Traces the build's processes from the thread it runs on: it seizes the
/// build command's process, follows every process and thread that process
/// starts, and tells `post` what each does, in the order it saw them, reading
/// what a process runs while the process waits for it. A process that execs
/// runs its program once the session has taken the command (`taken`): its
/// handlers read the command's files as they are before it runs.
class Tracer {
public:
    /// Of the build command's process `build`, which waits on `socket` until it
    /// is traced.
    Tracer(int build, int socket, std::function<void(Event)> post) :
        build(build), socket(socket), post(std::move(post)) {}

    /// Follows the build until the build command's process has exited, and
    /// takes what is waiting then. The rest of the processes then go on
    /// untraced, as the thread exits.
    void run();

    /// The session has taken the command of the last Execed posted.
    void taken() {
        took.release();
    }

private:
    /// Thread `tid` stopped, as `status` says.
    void stopped(int tid, int status, chrono::microseconds time);

    /// Thread `tid` started thread or process `child`, its process `process`.
    void created(int tid, int child, int process);

    /// Thread `tid` stopped before the event of the thread that started it.
    void arrived(int tid);

    /// Thread `tid` ended, as `status` says.
    void exited(int tid, int status, chrono::microseconds time);

    /// What process `pid` runs, which execed it.
    Execed execed(int pid, chrono::microseconds time) const;

private:
    int build;

    int socket;

    std::function<void(Event)> post;

    std::binary_semaphore took{0};

    /// The threads that run, by ID, each with the process it is of.
    std::unordered_map<int, int> tracees;

    /// The threads the event of the thread that started them named, with
    /// the process each is of, which have not stopped for the first time yet.
    std::unordered_map<int, int> announced;

    /// Whether catter runs as root, as whom a program runs with the privileges
    /// of its file.
    const bool root = ::geteuid() == 0;
};

void Tracer::run() {
    // catter-proxy may be in its own exec until it says it runs, which the
    // tracer would see as the build command.
    char running = 0;
    while(::read(socket, &running, 1) < 0 && errno == EINTR) {}
    if(running == 0) {
        post(Done{.error = std::format("{} ended before it ran the build command", PROXY)});
        return;
    }
    if(::ptrace(PTRACE_SEIZE, build, nullptr, reinterpret_cast<void*>(OPTIONS)) != 0) {
        post(Done{.error = denied(build, errno)});
        return;
    }
    tracees.emplace(build, build);
    ::shutdown(socket, SHUT_WR);
    std::optional<async::process::exit_status> ended;
    while(true) {
        int status = 0;
        // waitpid, not waitid, which takes __WALL from Linux 4.7 on.
        const int tid = ::waitpid(-1, &status, __WALL | __WNOTHREAD | (ended ? WNOHANG : 0));
        if(tid < 0 && errno == EINTR) {
            continue;
        }
        if(tid <= 0) {
            break;
        }
        const auto time = chrono::duration_cast<chrono::microseconds>(
            chrono::steady_clock::now().time_since_epoch());
        if(WIFSTOPPED(status)) {
            stopped(tid, status, time);
        } else {
            exited(tid, status, time);
            if(tid == build) {
                ended = exit_status_of(status);
            }
        }
    }
    assert(ended.has_value());
    post(Done{.status = *ended});
}

void Tracer::stopped(int tid, int status, chrono::microseconds time) {
    const int signal = WSTOPSIG(status);
    switch(const int event = status >> 16) {
        case PTRACE_EVENT_FORK:
        case PTRACE_EVENT_VFORK:
        case PTRACE_EVENT_CLONE: {
            unsigned long child = 0;
            ::ptrace(PTRACE_GETEVENTMSG, tid, nullptr, &child);
            const int id = static_cast<int>(child);
            if(!tracees.contains(id)) {
                created(tid, id, event == PTRACE_EVENT_CLONE ? lineage_of(id).process : id);
            }
            return resume(tid);
        }
        case PTRACE_EVENT_EXEC: {
            // A thread that execs takes the ID of its process's main thread.
            unsigned long former = 0;
            ::ptrace(PTRACE_GETEVENTMSG, tid, nullptr, &former);
            if(static_cast<int>(former) != tid) {
                tracees.erase(static_cast<int>(former));
            }
            post(execed(tid, time));
            took.acquire();
            return resume(tid);
        }
        case EVENT_STOP: {
            if(auto found = announced.find(tid); found != announced.end()) {
                tracees.emplace(tid, found->second);
                announced.erase(found);
                return resume(tid);
            }
            if(!tracees.contains(tid)) {
                return arrived(tid);
            }
            // A group-stop, which lasts until a SIGCONT, as job control
            // has it; or its end.
            if(is_stop_signal(signal)) {
                ::ptrace(PTRACE_LISTEN, tid, nullptr, nullptr);
                return;
            }
            return resume(tid);
        }
        default: return resume(tid, signal);
    }
}

void Tracer::created(int tid, int child, int process) {
    if(process == child) {
        post(Forked{.parent = tracees.at(tid), .child = child});
    }
    announced.emplace(child, process);
}

void Tracer::arrived(int tid) {
    // The thread that started it waits, at the event that tells of it, until
    // it is resumed: its process has run no other program meanwhile.
    const auto lineage = lineage_of(tid);
    if(lineage.process == tid) {
        post(Forked{.parent = lineage.parent, .child = tid});
    }
    tracees.emplace(tid, lineage.process);
    resume(tid);
}

void Tracer::exited(int tid, int status, chrono::microseconds time) {
    auto& threads = tracees.contains(tid) ? tracees : announced;
    auto found = threads.find(tid);
    if(found == threads.end()) {
        return;
    }
    const int process = found->second;
    threads.erase(found);
    if(process == tid) {
        post(Exited{.pid = tid, .status = exit_status_of(status), .time = time});
    }
}

Execed Tracer::execed(int pid, chrono::microseconds time) const {
    const auto proc = std::format("/proc/{}/", pid);
    Execed execed{.pid = pid, .time = time};
    auto& command = execed.command;
    std::string bytes;
    [[maybe_unused]] auto unread = fs::read_file(bytes, proc + "cmdline");
    command.args = strings_of(bytes);
    const auto line = ranges::to<std::string>(command.args | views::join_with(' '));
    const auto failed = [&](std::error_code error) {
        execed.error =
            std::format("catter cannot read `{}` as the system runs it: {}", line, error.message());
        return std::move(execed);
    };

    if(auto error = fs::read_file(bytes, proc + "environ")) {
        return failed(error);
    }
    command.env = strings_of(bytes);
    if(auto error = fs::read_symlink(command.cwd, proc + "cwd")) {
        return failed(error);
    }
    if(auto cwd = fs::status(proc + "cwd"); cwd && cwd->links == 0) {
        execed.error = std::format(
            "`{}` runs in a working directory catter cannot read, which the build may have removed: it runs uncaptured",
            line);
        return execed;
    }
    auto exe = fs::status(proc + "exe");
    if(!exe) {
        return failed(exe.error());
    }
    if(auto error = fs::read_file(bytes, proc + "stat")) {
        return failed(error);
    }
    auto path = string_at(pid, environment_end(bytes));
    if(!path) {
        return failed(path.error());
    }

    // A script, or a program the system runs with another, has the program
    // that runs it as the process's.
    const auto file =
        std::format("{}{}/{}", proc, fs::path::is_absolute(*path) ? "root" : "cwd", *path);
    if(auto named = fs::status(file); named && named->id == exe->id) {
        command.executable = std::move(*path);
    } else if(auto error = fs::read_symlink(command.executable, proc + "exe")) {
        return failed(error);
    }

    execed.unprivileged =
        !root && ((exe->permissions & 04000) != 0 || (exe->permissions & 02010) == 02010 ||
                  ::getxattr((proc + "exe").c_str(), "security.capability", nullptr, 0) > 0);
    return execed;
}

/// A build the tracer follows: its commands go to the session in the order
/// the tracer saw them, until the build command's process has exited.
class Capture {
public:
    explicit Capture(Session& session) : session(session) {}

    /// Runs `build`, traced: how it ended.
    async::task<async::process::exit_status> run(const Build& build);

private:
    /// What `event` tells the session.
    async::task<> take(Event event);

private:
    Session& session;

    /// On the tracer's clock, the steady clock's from its epoch.
    BuildProcesses processes{[this] { return session.new_id(); },
                             -session.time_of(chrono::steady_clock::time_point())};

    /// What the tracer told, which the session has not been told yet.
    std::deque<Event> events;

    /// Set once the tracer tells something.
    async::event arrived;

    /// The programs catter warned run without their privileges.
    std::unordered_set<std::string> warned;
};

async::task<async::process::exit_status> Capture::run(const Build& build) {
    int sockets[2];
    if(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets) != 0) {
        throw cpptrace::runtime_error(
            std::format("Failed to make the socket the build command waits on: {}",
                        std::error_code(errno, std::generic_category()).message()));
    }
    kotatsu::ScopeExit close([&] { ::close(sockets[0]); });
    ::fcntl(sockets[1], F_SETFD, 0);
    async::process::options options{
        .program = program_path(PROXY),
        .args = {resolve_program(build.command.front(), build.cwd)},
        .env_changes = {{.name = env::PTRACE, .value = std::to_string(sockets[1])}},
        .cwd = build.cwd,
    };
    options.args.append_range(build.command);
    auto child = sys::spawn(standing_in(std::move(options)));
    ::close(sockets[1]);
    if(!child) {
        throw cpptrace::runtime_error(
            std::format("Failed to start {}: {}", program_path(PROXY), child.error().message()));
    }
    const int pid = child->pid();
    processes.start(pid);
    std::optional<StandIn> stand_in(std::in_place, pid, false);
    const auto line = ranges::to<std::string>(build.command | views::join_with(' '));
    LOG_INFO("Starting the build command `{}` in {}, traced", line, build.cwd);

    auto relay = async::event_loop::current().create_relay();
    Tracer tracer(pid, sockets[0], [&](Event event) {
        relay.send([&, event = std::move(event)] mutable {
            // Once reaped, its pid may be another process's.
            if(auto* exited = std::get_if<Exited>(&event); exited && exited->pid == pid) {
                stand_in.reset();
            }
            events.push_back(std::move(event));
            arrived.set();
        });
    });
    std::jthread thread([&] { tracer.run(); });
    Done done;
    while(true) {
        while(events.empty()) {
            arrived.reset();
            co_await arrived.wait();
        }
        auto event = std::move(events.front());
        events.pop_front();
        if(auto* last = std::get_if<Done>(&event)) {
            done = std::move(*last);
            break;
        }
        const bool exec = std::holds_alternative<Execed>(event);
        co_await take(std::move(event));
        if(exec) {
            tracer.taken();
        }
    }
    thread.join();

    if(done.error) {
        [[maybe_unused]] auto killed = child->kill();
        [[maybe_unused]] auto reaped = child->wait();
        throw cpptrace::runtime_error(std::move(*done.error));
    }
    int failure = 0;
    if(::recv(sockets[0], &failure, sizeof(failure), MSG_DONTWAIT) == sizeof(failure)) {
        co_await session.uncaptured(
            session.new_id(),
            std::nullopt,
            std::format("Run `{}` failed:\n`{}` did not start: {}",
                        line,
                        build.command.front(),
                        std::error_code(failure, std::generic_category()).message()));
        co_return async::process::exit_status{.code = -1};
    }
    co_return done.status;
}

async::task<> Capture::take(Event event) {
    if(auto* forked = std::get_if<Forked>(&event)) {
        processes.fork(forked->parent, forked->child);
        co_return;
    }
    if(auto* exited = std::get_if<Exited>(&event)) {
        for(const auto& ended: processes.exit(exited->pid, exited->status, exited->time)) {
            co_await session.exited(ended.id, ended.result);
        }
        co_return;
    }
    auto& execed = std::get<Execed>(event);
    if(execed.unprivileged && warned.insert(execed.command.executable).second) {
        output::warn(
            "`{}` runs without the privileges its file grants it (set-user-ID, set-group-ID or capabilities), as every program catter traces does: capture a build that needs them with --capture hook",
            execed.command.executable);
    }
    auto started = processes.exec(execed.pid, execed.pid, std::move(execed.command), execed.time);
    // A process forked by one killed before its fork event, which the system
    // handed to another parent, is no process of the build's.
    if(!started) {
        co_return;
    }
    if(execed.error) {
        co_await session.uncaptured(started->id, started->parent, std::move(*execed.error));
    } else {
        co_await session.decide(started->id, started->parent, std::move(started->command));
    }
}

class PtraceBackend final : public Backend {
public:
    std::string_view name() const noexcept override {
        return "ptrace";
    }

    bool follows_decisions() const noexcept override {
        return false;
    }

    async::task<async::process::exit_status> run(const Build& build,
                                                 Session& session) const override {
        assert(!build.command.empty());
        Capture capture(session);
        co_return co_await capture.run(build);
    }
};

}  // namespace

const Backend& ptrace_backend() noexcept {
    const static PtraceBackend backend;
    return backend;
}

#endif

}  // namespace catter
