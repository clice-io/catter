module;

#include "modules/prelude.h"

#include "support/config/catter-proxy.h"
#include "support/config/ipc.h"
#include "support/config/session.h"
#include "support/messages.h"
#include "support/paths.h"

module catter;

import catter.command;
import :capture.backend;
import :session.ipc;
import :session.process;
import :session.session;

#ifndef CATTER_WINDOWS

namespace catter {
namespace {

/// The variables that name a program a build tool runs in place of the one it would find.
constexpr auto PROGRAM_VARIABLES = std::to_array<std::string_view>(
    {"CC", "CXX", "OBJC", "OBJCXX", "CUDACXX", "HIPCXX", "AR", "LD"});

/// CMake's languages whose compilers catter knows, each of which CMake runs through the program
/// CMAKE_<LANG>_COMPILER_LAUNCHER and CMAKE_<LANG>_LINKER_LAUNCHER name.
constexpr auto CMAKE_LANGUAGES =
    std::to_array<std::string_view>({"C", "CXX", "OBJC", "OBJCXX", "CUDA", "HIP"});

/// Whether catter knows the program `name` names, by its name: a compiler, a linker, a librarian
/// or an archiver.
bool known(std::string_view name) {
    return identify(name).has_value() || identify_tool(name).has_value() || is_archiver(name);
}

/// The shims of a session, catter-proxy under the name of each program it stands in for, in a
/// directory of their own, which goes with them.
class Shims {
public:
    explicit Shims(std::string directory) : dir(std::move(directory)) {
        // A directory a session of the same pid left behind.
        [[maybe_unused]] auto removed = kota::fs::remove_all(dir);
        if(auto created = kota::fs::create_directories(dir); !created) {
            throw cpptrace::runtime_error(
                std::format("Failed to create the directory of shims {}: {}",
                            dir,
                            created.error().message()));
        }
        link(config::LAUNCHER_NAME, dir);
    }

    Shims(const Shims&) = delete;
    Shims& operator= (const Shims&) = delete;

    ~Shims() {
        [[maybe_unused]] auto removed = kota::fs::remove_all(dir);
    }

    std::string_view directory() const noexcept {
        return dir;
    }

    /// The launcher, which stands in for the program its first argument names.
    std::string launcher() const {
        std::string path = dir;
        kota::fs::path::push(path, config::LAUNCHER_NAME);
        return path;
    }

    /// Adds a shim of each program in the directories of the list of paths `path_list` that catter
    /// knows by its name: the first of each name, as PATH finds it for a build that runs in `cwd`.
    void add_found(std::string_view path_list, std::string_view cwd) {
        for(auto entry_of_list: kota::sys::split_paths(path_list)) {
            std::string directory(cwd);
            kota::fs::path::push(directory, entry_of_list);
            auto entries = kota::fs::read_dir(directory);
            if(!entries) {
                continue;
            }
            for(const auto& entry: *entries) {
                if(!entry) {
                    break;
                }
                if(!known(entry->name) || names.contains(entry->name)) {
                    continue;
                }
                std::string program(directory);
                kota::fs::path::push(program, entry->name);
                if(auto status = kota::fs::status(program);
                   !status || status->type != kota::fs::FileType::Regular ||
                   (status->permissions & 0111) == 0) {
                    continue;
                }
                link(entry->name, dir);
            }
        }
    }

    /// The shim of `program`, a path, which runs that program wherever another of its name is:
    /// in a directory of its own, beside the file that names the program.
    std::string pin(std::string_view program) {
        pins += 1;
        std::string directory = dir;
        kota::fs::path::push(directory, std::to_string(pins));
        std::string named = directory;
        kota::fs::path::push(named, config::SHIM_PROGRAM_FILE);
        std::error_code error;
        if(auto created = kota::fs::create_directory(directory); !created) {
            error = created.error();
        } else {
            error = kota::fs::write_file(named, program);
        }
        if(error) {
            throw cpptrace::runtime_error(
                std::format("Failed to make a shim of {}: {}", program, error.message()));
        }
        return link(kota::fs::path::filename(program), directory);
    }

private:
    /// Links catter-proxy as `name` in `directory`: the path of the shim.
    std::string link(std::string_view name, std::string_view directory) {
        std::string shim(directory);
        kota::fs::path::push(shim, name);
        if(auto error = kota::fs::create_symlink(program_path(config::PROXY_EXE_NAME), shim)) {
            throw cpptrace::runtime_error(
                std::format("Failed to make the shim {}: {}", shim, error.message()));
        }
        if(directory == dir) {
            names.emplace(name);
        }
        return shim;
    }

private:
    std::string dir;

    /// The names of the shims in the directory.
    std::set<std::string, std::less<>> names;

    std::size_t pins = 0;
};

/// `value` of a variable that names a program to run, perhaps with arguments after it, which
/// runs the program's shim in its place: the shim of the program an absolute path names. A name is
/// looked for on PATH, where the shims are first; a relative path, which a build tool takes from
/// each directory it runs in, is left as it is.
std::string through_shim(std::string_view value, Shims& shims) {
    const auto end = value.find_first_of(" \t");
    const auto program = value.substr(0, end);
    if(!kota::fs::path::is_absolute(program) || !known(kota::fs::path::filename(program))) {
        return std::string(value);
    }
    auto shim = shims.pin(program);
    // Build tools split the variable at blanks, and run what comes before the first.
    if(shim.find_first_of(" \t") != std::string::npos) {
        throw cpptrace::runtime_error(std::format(
            "catter cannot name the shim {} in a variable such as CC, which build tools split at blanks: set CATTER_HOME to a directory whose path has none",
            shim));
    }
    if(end != std::string_view::npos) {
        shim += value.substr(end);
    }
    return shim;
}

/// The environment of a build under the shims of `shims`, which reports to the pipe `pipe`: this
/// process's, with the shims first on PATH, the programs variables such as CC name by their
/// paths run through theirs, and CMake running each compile and link through the launcher, unless
/// the build names a launcher of its own.
std::vector<kota::process::env_var> environment(Shims& shims,
                                                std::string_view pipe,
                                                std::string_view cwd) {
    std::vector<kota::process::env_var> env;
    std::optional<std::string> path;
    auto named = [](std::string_view name) {
        return [name](std::string_view other) {
            return kota::sys::env_name_equal(name, other);
        };
    };
    for(auto& [name, value]: kota::sys::environment()) {
        // The build command has no parent.
        if(kota::sys::env_name_equal(name, config::KEY_CATTER_COMMAND_ID)) {
            continue;
        }
        if(kota::sys::env_name_equal(name, "PATH")) {
            path = std::move(value);
            continue;
        }
        if(std::ranges::any_of(PROGRAM_VARIABLES, named(name))) {
            value = through_shim(value, shims);
        }
        env.push_back({.name = std::move(name), .value = std::move(value)});
    }

    if(!path) {
        throw cpptrace::runtime_error(
            "The wrapper backend puts its shims first on the build's PATH, which is not set: set it");
    }
    shims.add_found(*path, cwd);
    std::string shims_path;
    std::vector<std::string> directories = {std::string(shims.directory())};
    for(auto directory: kota::sys::split_paths(*path)) {
        directories.emplace_back(directory);
    }
    if(auto error = kota::sys::join_paths(shims_path, directories)) {
        throw cpptrace::runtime_error(std::format(
            "catter cannot put its shims {} on PATH, which splits paths at what that path holds: set CATTER_HOME to a directory whose path has none",
            shims.directory()));
    }
    env.push_back({.name = "PATH", .value = std::move(shims_path)});
    env.push_back({.name = config::KEY_CATTER_IPC, .value = std::string(pipe)});
    env.push_back({.name = config::KEY_CATTER_SHIMS, .value = std::string(shims.directory())});

    for(auto language: CMAKE_LANGUAGES) {
        for(auto kind: {"COMPILER", "LINKER"}) {
            auto name = std::format("CMAKE_{}_{}_LAUNCHER", language, kind);
            if(std::ranges::none_of(env, named(name), &kota::process::env_var::name)) {
                env.push_back({.name = std::move(name), .value = shims.launcher()});
            }
        }
    }
    return env;
}

class WrapperBackend final : public Backend {
public:
    std::string_view name() const noexcept override {
        return "wrapper";
    }

    kota::task<kota::process::exit_status> run(const Build& build,
                                               Session& session) const override {
        auto pipe_name = config::session_pipe_name();
        Shims shims(data_path(std::format("shims-{}", kota::sys::pid())));
        kota::process::options launch{
            .program = shims.launcher(),
            .args = build.command,
            .env = environment(shims, pipe_name, build.cwd),
            .cwd = build.cwd,
        };
        co_return co_await session.serve(
            pipe_name,
            [launch = std::move(launch)]() mutable {
                return run_process(std::move(launch), {.stand_in = true});
            },
            [&](data::ipcid_t id, kota::pipe&& client) {
                return ipc::accept(std::make_unique<ipc::Service>(session, id), std::move(client));
            });
    }
};

}  // namespace

const Backend& wrapper_backend() noexcept {
    const static WrapperBackend backend;
    return backend;
}

}  // namespace catter

#endif
