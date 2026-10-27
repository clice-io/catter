module;

#include "modules/prelude.h"

#if defined(CATTER_LINUX)
#include <sys/auxv.h>
#elif defined(CATTER_MAC)
#include <mach-o/dyld.h>
#endif

#include "support/config/catter-proxy.h"
#include "support/config/ipc.h"
#include "support/config/session.h"
#include "support/guard.h"
#include "support/log.h"
#include "support/messages.h"
#include "support/output.h"
#include "support/paths.h"
#include "support/program.h"

module catter;

#ifndef CATTER_WINDOWS
import :capture.inject.hook;
#endif
import :proxy.ipc;
import :proxy.option;
import :session.process;

using namespace catter;

namespace {
using catter::data::action;
using exit_status = kota::process::exit_status;

/// A command the proxy stands in for, and the session it reports it to.
struct Proxied {
    /// The session's pipe.
    std::string pipe;

    /// The command that started this one, 0 for the build command.
    data::ipcid_t parent = 0;

    /// The command as it runs here. Its working directory is empty when it
    /// cannot be read, as when the build removed it: the program starts in it
    /// all the same.
    data::command command;

    /// The options that run the command as catter decided, as command `id`.
    std::function<kota::process::options(data::command, data::ipcid_t)> launch;

    /// Why catter cannot capture the command, which the proxy reports in its
    /// place.
    std::optional<std::string> error;
};

kota::task<exit_status> run(data::action act,
                            data::ipcid_t id,
                            const Proxied& proxied,
                            [[maybe_unused]] proxy::Peer& peer) {
    using catter::data::action;

    RunOptions stand_in{.stand_in = true};
#ifdef CATTER_MAC
    stand_in.started = [&](int pid) {
        peer.started(pid);
    };
    auto exited = make_guard([&]() noexcept { peer.program.reset(); });
#endif
    switch(act.type) {
        case action::RUN: {
            co_return co_await run_process(proxied.launch(std::move(act.cmd), id),
                                           std::move(stand_in));
        }
        case action::DROP: {
            co_return exit_status{.code = 0};
        }
        case action::FAIL: {
            co_return exit_status{.code = 1};
        }
    }
    std::unreachable();
}

/// This process's environment, as a command carries it.
std::vector<std::string> environment() {
    std::vector<std::string> env;
    for(const auto& [name, value]: kota::sys::environment()) {
        env.push_back(std::format("{}={}", name, value));
    }
    return env;
}

/// This process's working directory; empty when it cannot be read.
std::string working_directory() {
    std::string cwd;
    if(kota::fs::current_dir(cwd)) {
        cwd.clear();
    }
    return cwd;
}

#ifndef CATTER_WINDOWS
/// The command the hook started the proxy for, which runs with the hook, or
/// what the hook found wrong.
Proxied hooked(const catter::proxy::ProxyOption& opt) {
    if(opt.error_msg.has_value() && !opt.args.has_value()) {
        return {.pipe = *opt.ipc, .parent = *opt.parent_id, .error = *opt.error_msg};
    }
    if(!opt.args.has_value()) {
        throw cpptrace::runtime_error("missing command arguments after --");
    }

    data::command cmd = {
        .cwd = working_directory(),
        .args = *opt.args,
        .env = environment(),
    };
    cmd.executable = opt.exec.has_value() ? *opt.exec : resolve_program(cmd.args.at(0));
    return {
        .pipe = *opt.ipc,
        .parent = *opt.parent_id,
        .command = std::move(cmd),
        .launch =
            [pipe = *opt.ipc](data::command command, data::ipcid_t id) {
                return hook_options(std::move(command), id, pipe);
            },
    };
}
#endif

/// The value of variable `name` in `env`, entries of which are `name=value`.
std::optional<std::string_view> variable(std::span<const std::string> env, std::string_view name) {
    for(std::string_view entry: env) {
        if(entry.size() > name.size() && entry[name.size()] == '=' &&
           kota::sys::env_name_equal(entry.substr(0, name.size()), name)) {
            return entry.substr(name.size() + 1);
        }
    }
    return std::nullopt;
}

/// Sets variable `name` of `env` to `value`.
void set_variable(std::vector<std::string>& env, std::string_view name, std::string_view value) {
    auto entry = std::ranges::find_if(env, [&](std::string_view entry) {
        return entry.size() > name.size() && entry[name.size()] == '=' &&
               kota::sys::env_name_equal(entry.substr(0, name.size()), name);
    });
    auto assignment = std::format("{}={}", name, value);
    if(entry == env.end()) {
        env.push_back(std::move(assignment));
    } else {
        *entry = std::move(assignment);
    }
}

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
        kota::fs::path::push(named, config::SHIM_DIRECTORY_FILE);
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
    std::string pipe(variable(cmd.env, config::KEY_CATTER_IPC).value_or(""));
    std::string shims(
        variable(cmd.env, config::KEY_CATTER_SHIMS).value_or(kota::fs::path::parent(self)));
    data::ipcid_t parent = 0;
    if(auto id = variable(cmd.env, config::KEY_CATTER_COMMAND_ID)) {
        std::from_chars(id->data(), id->data() + id->size(), parent);
    }
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
            .pipe = std::move(pipe),
            .parent = parent,
            .error = std::format("`{}` is found nowhere past catter's shims", program),
        };
    }
    // A build tool that ran the shim by its path does not see the shim's: it
    // ran the program.
    if(is_shim(args.front(), shims)) {
        args.front() = *real;
    }
    const bool build = launcher && parent == 0 && !pipe.empty();
    if(!build && path) {
        std::string without;
        // Entries PATH was split into join back.
        [[maybe_unused]] auto error = kota::sys::join_paths(without, past_shims(*path, shims));
        assert(!error);
        set_variable(cmd.env, "PATH", without);
    }
    cmd.executable = std::move(*real);
    cmd.args = std::move(args);
    return {
        .pipe = std::move(pipe),
        .parent = parent,
        .command = std::move(cmd),
        .launch =
            [](data::command command, data::ipcid_t id) {
                set_variable(command.env, config::KEY_CATTER_COMMAND_ID, std::to_string(id));
                return process_options(command);
            },
    };
}

std::string format_args(std::span<const std::string> argv) {
    std::string result;
    for(const auto& arg: argv) {
        if(!result.empty()) {
            result += ' ';
        }
        result += arg;
    }
    return result;
}

kota::task<exit_status> handle_proxy_request(std::span<const std::string> argv,
                                             const Proxied& proxied,
                                             proxy::Peer& peer) noexcept {
    // ensure peer is closed when proxy_main exits, otherwise the peer might still be
    // running and trying to access resources that have been cleaned up after proxy_main
    // exits.
    auto guard = make_guard([&]() noexcept {
        auto err = peer.close();
        if(err.has_error()) {
            LOG_ERROR("Failed to close IPC peer: {}", err.error().message);
        }
    });

    std::string err;

    try {
        if(proxied.error) {
            co_await peer.report_error(proxied.parent, *proxied.error);
            co_return exit_status{.code = -1};
        }
        const auto& cmd = proxied.command;
        if(cmd.cwd.empty()) {
            co_await peer.report_error(
                proxied.parent,
                std::format(
                    "`{}` runs in a working directory catter cannot read, which the build may have removed: it runs uncaptured",
                    format_args(cmd.args)));
            co_return co_await run_process(process_options(cmd), {.stand_in = true});
        }

        auto id = co_await peer.create(proxied.parent);

        auto received_act = co_await peer.make_decision(cmd);

        auto status = co_await run(received_act, id, proxied, peer);

        // A command can outlive its session (a build server): its result
        // stands, though catter is gone.
        try {
            co_await peer.finish(result_of(status));
        } catch(const std::exception& ex) {
            LOG_WARN("Command {} ended after its session: {}", id, ex.what());
        }

        co_return status;
    } catch(const cpptrace::exception& ex) {
        err = std::format("{}\n", ex.message());
        LOG_ERROR("{}\nStack trace:\n{}", ex.message(), ex.trace().to_string());
    } catch(const std::exception& ex) {
        err = std::format("{}\n", ex.what());
    } catch(...) {
        err = "Unknown fatal error.\n";
    }

    auto msg = std::format("Run `{}` failed:\n{}", format_args(argv), err);

    LOG_ERROR(msg);
    co_await peer.report_error(proxied.parent, msg);

    co_return exit_status{.code = -1};
};

#ifdef CATTER_MAC
/// Runs the connection to catter. macOS ends no child with its parent: the
/// build command ends here when catter ends while it runs, and with catter its
/// connection.
kota::task<> run_connection(proxy::Peer& peer, bool build_command) {
    co_await peer.run();
    if(build_command && peer.program) {
        ::kill(static_cast<pid_t>(*peer.program), SIGKILL);
    }
}
#endif

/// Stands in for the command of `proxied`: reports it to its session, and runs
/// it as decided, or as it is when there is no session.
kota::task<exit_status> proxy_main(std::span<const std::string> argv,
                                   const Proxied& proxied) noexcept {
    auto& current = kota::event_loop::current();
    auto ret = co_await kota::pipe::connect(proxied.pipe, kota::pipe::options(), current);
    if(!ret) {
        // A build server an earlier session started (an sccache server, a
        // Gradle daemon) outlives that session's pipe: its commands run as
        // they would without catter.
        LOG_WARN("Failed to connect to IPC pipe: {}, error: {}; running the command uncaptured",
                 proxied.pipe,
                 ret.error().message());
        if(proxied.error) {
            co_return exit_status{.code = -1};
        }
        try {
            co_return co_await run_process(process_options(proxied.command), {.stand_in = true});
        } catch(const std::exception& ex) {
            LOG_ERROR("Run `{}` failed: {}", format_args(argv), ex.what());
            co_return exit_status{.code = -1};
        }
    }
    auto peer = proxy::Peer{
        kota::ipc::BincodePeer{current,
                               std::make_unique<kota::ipc::StreamTransport>(std::move(*ret))}
    };

#ifdef CATTER_MAC
    // The build command's proxy is started with parent 0.
    auto connection = run_connection(peer, proxied.parent == 0);
#else
    auto connection = peer.run();
#endif
    auto [_, status] =
        co_await kota::when_all{std::move(connection), handle_proxy_request(argv, proxied, peer)};

    co_return status;
}

void init_logger(std::string_view pipe) {
    // No session, no data directory of its.
    if(pipe.empty()) {
        log::mute_logger();
        return;
    }
    try {
#ifdef CATTER_WINDOWS
        // The session's pipe is a named pipe, in no directory.
        auto path = data_path(config::PROXY_LOG_PATH);
#else
        auto path = config::session_data_path(pipe, config::PROXY_LOG_PATH);
#endif
        log::init_logger("catter-proxy.log", path, false);
    } catch(const std::exception&) {
        log::mute_logger();
    }
}

/// The path this program was started by: on Linux and macOS a symlink to it
/// among the wrapper backend's shims, which kota::sys::executable_path
/// resolves; on Windows a hard link, which it does not.
std::string started_by() {
#if defined(CATTER_WINDOWS)
    std::string path;
    [[maybe_unused]] auto error = kota::sys::executable_path(path);
    return path;
#elif defined(CATTER_LINUX)
    return reinterpret_cast<const char*>(::getauxval(AT_EXECFN));
#elif defined(CATTER_MAC)
    std::uint32_t size = 0;
    ::_NSGetExecutablePath(nullptr, &size);
    std::string path(size, '\0');
    ::_NSGetExecutablePath(path.data(), &size);
    path.resize(std::strlen(path.c_str()));
    return path;
#endif
}

int run_proxied(std::span<const std::string> argv, const Proxied& proxied) {
    init_logger(proxied.pipe);
    auto [status] = kota::run(proxy_main(argv, proxied));
    return status.has_value() ? exit_like(*status) : -1;
}

// we do not output in proxy, it must be invoked by main program.
// usage: catter-proxy -p <parent ipc id> [--exec <exe path>] -- <args...>
// TODO: act as a fake compiler
int run(std::span<const std::string> argv) {
    hold_closed_streams();

    // Among the wrapper backend's shims, catter-proxy is the launcher, or a
    // shim by the name of the program it stands in for.
    const auto self = started_by();
#ifdef CATTER_WINDOWS
    // Windows names a program with its extension, which the role's name has
    // not.
    const auto name = kota::fs::path::stem(self);
#else
    const auto name = kota::fs::path::filename(self);
#endif
    if(name == config::LAUNCHER_NAME) {
        std::vector<std::string> args(argv.begin() + 1, argv.end());
        std::string program = args.front();
        return run_proxied(argv, wrapped(self, program, std::move(args), true));
    }
    if(name != config::PROXY_EXE_NAME) {
        return run_proxied(
            argv,
            wrapped(self, self, std::vector<std::string>(argv.begin(), argv.end()), false));
    }

#ifdef CATTER_WINDOWS
    // The hook on Windows starts each program itself.
    return -1;
#else

    kota::deco::cli::Command<catter::proxy::Option> cli(
        "Catter Proxy, the tool for receive hook info and send it to catter.");

    int ret = 0;
    std::vector<std::string> args(argv.begin() + 1, argv.end());
    cli.match(catter::proxy::Option::Cate::help,
              [&](const catter::proxy::Option& opt) { cli.usage(std::cerr); })
        .match(catter::proxy::Option::Cate::proxy,
               [&](const auto& opt) { ret = run_proxied(argv, hooked(opt.proxy_opt)); })
        .on_error([&](const kota::deco::cli::ParseError& err) {
            std::cerr << err.message << std::endl;
            ret = -1;
        })
        .execute(args);
    return ret;
#endif
}

}  // namespace

extern "C++" int main(int argc, char* argv[]) {
#ifdef CATTER_WINDOWS
    // main's argv is in the ANSI code page, which holds few characters past
    // ASCII.
    return run(kota::sys::arguments());
#else
    return run(std::vector<std::string>(argv, argv + argc));
#endif
}
