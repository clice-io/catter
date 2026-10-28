module;

#include "modules/prelude.h"

#include "support/log.h"
#include "support/messages.h"
#include "support/paths.h"

#ifdef CATTER_LINUX
#include <fcntl.h>
#include <unistd.h>

#include "support/hook.h"
#endif

module catter;

#ifndef CATTER_WINDOWS
import :capture.inject;
#endif
import :session.process;

using namespace catter;

namespace {

using exit_status = kota::process::exit_status;

/// A command the proxy stands in for, and the session it reports it to.
struct Proxied {
    /// The session's pipe; empty when there is none.
    std::string pipe;

    /// The command that started this one, none for the build command.
    std::optional<data::ipcid_t> parent;

    /// The command as it runs here, without catter's variables. Its working
    /// directory is empty when it cannot be read, as when the build removed
    /// it: the program starts in it all the same.
    data::command command;

    /// The options that run the command as catter decided, as command `id`.
    std::function<kota::process::options(data::command, data::ipcid_t)> launch;

    /// Why catter cannot capture the command, which the proxy reports in its
    /// place.
    std::optional<std::string> error;
};

/// This process's working directory; empty when it cannot be read.
std::string working_directory() {
    std::string cwd;
    if(kota::fs::current_dir(cwd)) {
        cwd.clear();
    }
    return cwd;
}

/// Whether `entry`, `name=value`, is of variable `name`.
bool is_entry_of(std::string_view entry, std::string_view name) {
    return entry.size() > name.size() && entry[name.size()] == '=' &&
           kota::sys::env_name_equal(entry.substr(0, name.size()), name);
}

/// The value of variable `name` in `env`.
std::optional<std::string_view> variable(std::span<const std::string> env, std::string_view name) {
    auto entry =
        std::ranges::find_if(env, [&](std::string_view entry) { return is_entry_of(entry, name); });
    if(entry == env.end()) {
        return std::nullopt;
    }
    return std::string_view(*entry).substr(name.size() + 1);
}

/// The value of variable `name` in `env`, which it leaves out of `env`.
std::optional<std::string> take_variable(std::vector<std::string>& env, std::string_view name) {
    auto value =
        variable(env, name).transform([](std::string_view value) { return std::string(value); });
    std::erase_if(env, [&](std::string_view entry) { return is_entry_of(entry, name); });
    return value;
}

/// catter's variables in the environment `env` of a program of the build,
/// which name the session it reports to, taken out of `env`: they reach a
/// command only as catter-proxy starts it.
struct Variables {
    std::string pipe;

    std::optional<data::ipcid_t> parent;

    /// The directory of the wrapper backend's shims, when the build runs under
    /// it.
    std::optional<std::string> shims;

    /// The hook of a program no catter-proxy started restarts it through this
    /// one, as a command reported late.
    bool late;

    explicit Variables(std::vector<std::string>& env) :
        pipe(take_variable(env, env::IPC).value_or("")), shims(take_variable(env, env::SHIMS)),
        late(take_variable(env, env::LATE).has_value()) {
        if(auto id = take_variable(env, env::COMMAND_ID)) {
            data::ipcid_t value = 0;
            if(std::from_chars(id->data(), id->data() + id->size(), value).ec == std::errc()) {
                parent = value;
            }
        }
        take_variable(env, env::PROXY);
    }
};

#ifndef CATTER_WINDOWS
/// The command the hook, or catter for the build command, started catter-proxy
/// for, `catter-proxy <program> <argv>...`, which runs with the hook.
Proxied hooked(std::span<const std::string> argv) {
    data::command cmd{
        .cwd = working_directory(),
        .executable = argv[1],
        .args = {argv.begin() + 2, argv.end()},
        .env = environment(),
    };
    Variables variables(cmd.env);
    return {
        .pipe = variables.pipe,
        .parent = variables.parent,
        .command = std::move(cmd),
        .launch =
            [pipe = variables.pipe, late = variables.late](data::command command,
                                                           data::ipcid_t id) {
#ifdef CATTER_LINUX
                auto decided = late ? hook::decided_exec(kota::sys::pid())
                                    : hook::decided_exec(kota::sys::pid(), command.args);
#endif
                auto options = hook_options(std::move(command), id, pipe);
#ifdef CATTER_LINUX
                options.env->push_back({.name = env::DECIDED, .value = std::move(decided)});
#endif
                return options;
            },
    };
}
#endif

bool has_separator(std::string_view path) {
    return std::ranges::any_of(path, [](char c) { return kota::fs::path::is_separator(c); });
}

/// Whether `path` names one of the shims in the directory `shims`.
bool is_shim(std::string_view path, std::string_view shims) {
    return has_separator(path) && kota::fs::path::is_within(path, shims);
}

/// The entries of the list of paths `path_list` but the directories of shims
/// under `shims`.
std::vector<std::string> past_shims(std::string_view path_list, std::string_view shims) {
    std::vector<std::string> directories;
    for(auto directory: kota::sys::split_paths(path_list)) {
        if(!kota::fs::path::is_within(directory, shims)) {
            directories.emplace_back(directory);
        }
    }
    return directories;
}

/// The file the system starts for `program`, a name or a path, from a PATH of
/// `path_list`, as it would had catter put no shims in the way: for a shim in
/// a directory that stands for another, the program of its name there, else
/// the first of the shim's name on PATH past the shims. None when there is
/// none.
std::optional<std::string> resolve_past(std::string_view program,
                                        std::string_view shims,
                                        std::string_view path_list) {
    std::string_view name = program;
    if(has_separator(program)) {
        if(!is_shim(program, shims)) {
            return std::string(program);
        }
        name = kota::fs::path::filename(program);
        std::string named(kota::fs::path::parent(program));
        kota::fs::path::push(named, SHIM_DIRECTORY_FILE);
        if(std::string directory; !kota::fs::read_file(directory, named)) {
            kota::fs::path::push(directory, name);
            return directory;
        }
    }
    kota::sys::ProgramSearch search{.directories = past_shims(path_list, shims)};
    std::string found;
    // No directories would search the shims' PATH.
    if(search.directories.empty() || kota::sys::find_program(found, name, search)) {
        return std::nullopt;
    }
    return found;
}

/// The command a shim, or the `launcher`, stands in for: `args` as the build
/// started it, which runs `program`, the shim itself for a shim. The build
/// command, which catter starts through the launcher, runs with the shims on
/// its PATH; the commands under it run without them, captured or not, so that
/// the compilers a build tool runs do not find the shims again, nor a build
/// cache's compiler in their place.
Proxied wrapped(std::string_view self,
                std::string_view program,
                std::vector<std::string> args,
                bool launcher) {
    data::command cmd{.cwd = working_directory(), .env = environment()};
    Variables variables(cmd.env);
    const std::string shims = variables.shims.value_or(std::string(kota::fs::path::parent(self)));
    std::optional<std::string> path(variable(cmd.env, "PATH"));

    auto real = resolve_past(program, shims, path.value_or(""));
    // A path the build spelled otherwise than catter, which names a shim all
    // the same, would run it again and again.
    std::string executable;
    if(real && !kota::sys::executable_path(executable)) {
        if(auto same = kota::fs::equivalent(*real, executable); same && *same) {
            real.reset();
        }
    }
    if(!real) {
        return {
            .pipe = variables.pipe,
            .parent = variables.parent,
            .error = std::format("`{}` is found nowhere past catter's shims", program),
        };
    }
    // A build tool that ran the shim by its path does not see the shim's: it
    // ran the program.
    if(is_shim(args.front(), shims)) {
        args.front() = *real;
    }
    const bool build = launcher && !variables.parent && !variables.pipe.empty();
    if(!build && path) {
        std::string without;
        // Entries PATH was split into join back.
        [[maybe_unused]] auto error = kota::sys::join_paths(without, past_shims(*path, shims));
        assert(!error);
        std::ranges::replace_if(
            cmd.env,
            [](std::string_view entry) { return is_entry_of(entry, "PATH"); },
            "PATH=" + without);
    }
    cmd.executable = std::move(*real);
    cmd.args = std::move(args);
    return {
        .pipe = variables.pipe,
        .parent = variables.parent,
        .command = std::move(cmd),
        .launch =
            [variables](data::command command, data::ipcid_t id) {
                command.env.push_back(std::format("{}={}", env::IPC, variables.pipe));
                if(variables.shims) {
                    command.env.push_back(std::format("{}={}", env::SHIMS, *variables.shims));
                }
                command.env.push_back(std::format("{}={}", env::COMMAND_ID, id));
                return process_options(command);
            },
    };
}

/// The connection to the session.
struct Connection {
    kota::ipc::BincodePeer peer;

#ifdef CATTER_MAC
    /// The program the proxy stands in for, from its start until its exit.
    std::optional<std::int64_t> program;
#endif

    /// Sends request `Type`: its result. Throws when the session fails it, or
    /// is gone.
    template <ipc::RequestType Type>
    kota::task<typename ipc::Request<Type>::Result>
        request(const typename ipc::Request<Type>::Params& params) {
        using Request = ipc::Request<Type>;
        auto result = co_await peer.send_request<typename Request::Result>(Request::method, params);
        if(!result) {
            throw cpptrace::runtime_error(
                std::format("IPC request failed: {}", result.error().message));
        }
        co_return std::move(*result);
    }

    /// Tells the session that catter cannot capture the command, as `message`
    /// says; it is lost when the session is gone.
    kota::task<> report_error(std::optional<data::ipcid_t> parent, std::string message) {
        try {
            co_await request<ipc::RequestType::REPORT_ERROR>(
                {.parent = parent, .message = std::move(message)});
        } catch(const std::exception& ex) {
            LOG_WARN("Failed to report an uncaptured command: {}", ex.what());
        }
    }
};

/// Runs `command` as `action` says, as command `id`: how it ended.
kota::task<exit_status> run(data::action action,
                            data::ipcid_t id,
                            const Proxied& proxied,
                            [[maybe_unused]] Connection& connection) {
    RunOptions stand_in{.stand_in = true, .release_streams = true};
#ifdef CATTER_MAC
    stand_in.started = [&](int pid) {
        connection.program = pid;
        using Started = ipc::Request<ipc::RequestType::STARTED>;
        // The program runs all the same.
        [[maybe_unused]] auto sent =
            connection.peer.send_notification(Started::method, Started::Params{pid});
    };
    kota::ScopeExit exited([&] { connection.program.reset(); });
#endif
    switch(action.type) {
        case data::action::RUN: {
            co_return co_await run_process(proxied.launch(std::move(action.cmd), id),
                                           std::move(stand_in));
        }
        case data::action::DROP: {
            co_return exit_status{.code = 0};
        }
        case data::action::FAIL: {
            co_return exit_status{.code = 1};
        }
    }
    std::unreachable();
}

/// Reports the command of `proxied` to its session, and runs it as decided.
kota::task<exit_status> report(const Proxied& proxied, Connection& connection) noexcept {
    kota::ScopeExit close([&] {
        if(auto closed = connection.peer.close(); !closed) {
            LOG_ERROR("Failed to close the connection to the session: {}", closed.error().message);
        }
    });

    const auto& command = proxied.command;
    const auto line = std::ranges::to<std::string>(command.args | std::views::join_with(' '));
    std::string error;
    try {
        if(proxied.error) {
            co_await connection.report_error(proxied.parent, *proxied.error);
            co_return exit_status{.code = -1};
        }
        if(command.cwd.empty()) {
            co_await connection.report_error(
                proxied.parent,
                std::format(
                    "`{}` runs in a working directory catter cannot read, which the build may have removed: it runs uncaptured",
                    line));
            co_return co_await run_process(process_options(command),
                                           {.stand_in = true, .release_streams = true});
        }

        auto [id, action] = co_await connection.request<ipc::RequestType::MAKE_DECISION>(
            {.parent = proxied.parent, .command = command});
        const auto start = std::chrono::steady_clock::now();
        auto status = co_await run(std::move(action), id, proxied, connection);
        const auto result = result_of(status, start);

        // A command can outlive its session (a build server): its result
        // stands, though catter is gone.
        try {
            co_await connection.request<ipc::RequestType::FINISH>({
                .code = result.code,
                .started =
                    std::chrono::duration_cast<std::chrono::microseconds>(start.time_since_epoch())
                        .count(),
                .duration = result.duration,
            });
        } catch(const std::exception& ex) {
            LOG_WARN("Command {} ended after its session: {}", id, ex.what());
        }
        co_return status;
    } catch(const cpptrace::exception& ex) {
        error = ex.message();
        LOG_ERROR("{}\nStack trace:\n{}", ex.message(), ex.trace().to_string());
    } catch(const std::exception& ex) {
        error = ex.what();
    }

    co_await connection.report_error(proxied.parent,
                                     std::format("Run `{}` failed:\n{}\n", line, error));
    co_return exit_status{.code = -1};
}

/// Stands in for the command of `proxied`: reports it to its session, and runs
/// it as decided, or as it is when there is no session.
kota::task<exit_status> stand_in(const Proxied& proxied) noexcept {
    auto& loop = kota::event_loop::current();
    auto pipe = co_await kota::pipe::connect(proxied.pipe, kota::pipe::options(), loop);
    if(!pipe) {
        // A build server an earlier session started (an sccache server, a
        // Gradle daemon) outlives that session's pipe: its commands run as
        // they would without catter.
        LOG_WARN("Failed to connect to the session at `{}`: {}; the command runs uncaptured",
                 proxied.pipe,
                 pipe.error().message());
        if(proxied.error) {
            co_return exit_status{.code = -1};
        }
        try {
            co_return co_await run_process(process_options(proxied.command),
                                           {.stand_in = true, .release_streams = true});
        } catch(const std::exception& ex) {
            LOG_ERROR("{}", ex.what());
            co_return exit_status{.code = -1};
        }
    }
    Connection connection{
        .peer =
            kota::ipc::BincodePeer(loop,
                                   std::make_unique<kota::ipc::StreamTransport>(std::move(*pipe))),
    };

    auto run_connection = [](Connection& connection,
                             [[maybe_unused]] bool build_command) -> kota::task<> {
        co_await connection.peer.run();
#ifdef CATTER_MAC
        // macOS ends no child with its parent: the build command ends here
        // when catter ends while it runs, and with catter its connection.
        if(build_command && connection.program) {
            ::kill(static_cast<pid_t>(*connection.program), SIGKILL);
        }
#endif
    }(connection, !proxied.parent);
    auto [_, status] =
        co_await kota::when_all{std::move(run_connection), report(proxied, connection)};
    co_return status;
}

int run_proxied(const Proxied& proxied) {
    // No session, no data directory of its.
    if(proxied.pipe.empty()) {
        log::mute_logger();
    } else {
        try {
            log::init_logger("catter-proxy", data_path(proxied.pipe, "log/catter-proxy.log"));
        } catch(const std::exception&) {
            log::mute_logger();
        }
    }
    auto [status] = kota::run(stand_in(proxied));
    return status.has_value() ? exit_like(*status) : -1;
}

/// catter-proxy stands in for the commands of the build: by its own name for
/// the program the hook was asked to start, and among the wrapper backend's
/// shims as the launcher or as the program a shim is named after.
int run(std::span<const std::string> argv) {
    prepare_descriptors();

    // The link this program was started by, which kota::sys::executable_path
    // would resolve.
    std::string self;
    kota::sys::started_path(self);
#ifdef CATTER_WINDOWS
    // Windows names a program with its extension, which the role's name has
    // not.
    const auto name = kota::fs::path::stem(self);
#else
    const auto name = kota::fs::path::filename(self);
#endif
#ifndef CATTER_WINDOWS
    if(name == PROXY) {
        if(argv.size() < 2) {
            LOG_ERROR("catter-proxy runs a program for the hook: catter-proxy <program> <argv>...");
            return 1;
        }
        return run_proxied(hooked(argv));
    }
#endif
    if(name == LAUNCHER) {
        std::vector<std::string> args(argv.begin() + 1, argv.end());
        std::string program = args.front();
        return run_proxied(wrapped(self, program, std::move(args), true));
    }
    return run_proxied(wrapped(self, self, {argv.begin(), argv.end()}, false));
}

#ifdef CATTER_LINUX
/// The build command the ptrace backend traces, `catter-proxy <program>
/// <argv>...`, which catter started with `socket`, over which this process
/// tells catter it runs: once catter traces it, the program runs in its place,
/// or this process tells catter why it did not start.
[[noreturn]] void run_traced(int socket, char* argv[]) {
    kota::sys::unset_env(env::PTRACE);
    char byte = 1;
    [[maybe_unused]] auto told = ::write(socket, &byte, 1);
    while(::read(socket, &byte, 1) < 0 && errno == EINTR) {}
    ::fcntl(socket, F_SETFD, FD_CLOEXEC);
    // A name resolve_program found nowhere on PATH is not one in this
    // directory.
    errno = ENOENT;
    if(std::string_view(argv[1]).contains('/')) {
        ::execve(argv[1], argv + 2, environ);
    }
    if(errno == ENOEXEC) {
        // A file of no format the system knows runs by /bin/sh, as execvp runs
        // it.
        std::vector<char*> shell = {const_cast<char*>("/bin/sh"), argv[1]};
        for(auto arg = argv + 3; *arg != nullptr; arg += 1) {
            shell.push_back(*arg);
        }
        shell.push_back(nullptr);
        ::execve(shell.front(), shell.data(), environ);
    }
    const int error = errno;
    [[maybe_unused]] auto written = ::write(socket, &error, sizeof(error));
    // LeakSanitizer, which a Debug catter-proxy runs at its exit, fails under
    // ptrace.
    ::_exit(127);
}
#endif

}  // namespace

extern "C++" int main(int argc, char* argv[]) {
#ifdef CATTER_WINDOWS
    // main's argv is in the ANSI code page, which holds few characters past
    // ASCII.
    return run(kota::sys::arguments());
#else
#ifdef CATTER_LINUX
    if(std::string socket; kota::sys::get_env(socket, env::PTRACE)) {
        int fd = -1;
        std::from_chars(socket.data(), socket.data() + socket.size(), fd);
        run_traced(fd, argv);
    }
#endif
    return run(std::vector<std::string>(argv, argv + argc));
#endif
}
