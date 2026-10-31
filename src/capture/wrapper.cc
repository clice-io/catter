module;

#include "support/messages.h"
#include "support/paths.h"
#include "support/prelude.h"

module catter;

import catter.command;
import :capture.backend;
import :capture.ipc;
import :session.process;
import :session.session;

namespace catter {
namespace {

/// The variables that name a program a build tool runs in place of the one it would find.
constexpr auto PROGRAM_VARIABLES = std::to_array<std::string_view>(
    {"CC", "CXX", "OBJC", "OBJCXX", "CUDACXX", "HIPCXX", "AR", "LD"});

#ifdef CATTER_WINDOWS
/// The tools MSBuild runs, cl's compiler, linker and librarian, each from the directory a property
/// such as CLToolPath names, which MSBuild takes from the environment.
constexpr auto MSBUILD_TOOLS = std::to_array<std::string_view>({"cl.exe", "link.exe", "lib.exe"});
constexpr auto MSBUILD_PATHS =
    std::to_array<std::string_view>({"CLToolPath", "LinkToolPath", "LibToolPath"});
#endif

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
        [[maybe_unused]] auto removed = fs::remove_all(dir);
        if(auto created = fs::create_directories(dir); !created) {
            throw cpptrace::runtime_error(
                std::format("Failed to create the directory of shims {}: {}",
                            dir,
                            created.error().message()));
        }
#ifdef CATTER_WINDOWS
        // The shims are hard links, which a copy, on a volume other than catter's, takes.
        const auto proxy = program_path(std::format("{}.exe", PROXY));
        const auto launcher = this->launcher();
        if(fs::create_hard_link(proxy, launcher)) {
            if(auto error = fs::copy_file(proxy, launcher)) {
                throw cpptrace::runtime_error(
                    std::format("Failed to copy {} to {}: {}", proxy, launcher, error.message()));
            }
        }
        // MSBuild runs its compiler, linker and librarian from here, and these shims find them on
        // the PATH it sets for them.
        for(auto name: MSBUILD_TOOLS) {
            link(name, dir);
        }
#else
        link(LAUNCHER, dir);
#endif
    }

    Shims(const Shims&) = delete;
    Shims& operator= (const Shims&) = delete;

    ~Shims() {
        [[maybe_unused]] auto removed = fs::remove_all(dir);
    }

    std::string_view directory() const noexcept {
        return dir;
    }

    /// The launcher, which stands in for the program its first argument names.
    std::string launcher() const {
        std::string path = dir;
#ifdef CATTER_WINDOWS
        fs::path::push(path, std::format("{}.exe", LAUNCHER));
#else
        fs::path::push(path, LAUNCHER);
#endif
        return path;
    }

    /// The directory of the shims of the programs catter knows in `directory`, an entry of PATH,
    /// which goes right before it on the build's: whichever names a build tool looks for, and in
    /// whichever order, it finds the program it would without catter. None when `directory` holds
    /// none, or is relative, which names another directory wherever the build goes.
    std::optional<std::string> add_found(std::string_view directory) {
        if(!fs::path::is_absolute(directory)) {
            return std::nullopt;
        }
        if(auto found = on_path.find(directory); found != on_path.end()) {
            return found->second;
        }
        std::optional<std::string> shims;
        if(auto entries = fs::Directory::open(directory)) {
            for(const auto& entry: *entries) {
                if(!entry) {
                    break;
                }
                if(!known(entry->name)) {
                    continue;
                }
                std::string program(directory);
                fs::path::push(program, entry->name);
                if(!runs(program)) {
                    continue;
                }
                if(!shims) {
                    shims = standing_for(directory);
                }
                std::string shim = link(entry->name, *shims);
                names.try_emplace(entry->name, std::move(program), std::move(shim));
            }
        }
        on_path.emplace(directory, shims);
        return shims;
    }

    /// The shim of `program`, a path: the one PATH finds first under its name when that is the
    /// program's, else the one in the directory that stands for the program's, where each program
    /// of the program's directory is too, a shim of each one catter knows and a link to each
    /// other, as a build tool looks for the tools that go with a compiler beside it.
    std::string shim_of(std::string_view program) {
        const auto name = fs::path::filename(program);
        if(auto found = names.find(name); found != names.end()) {
            if(auto same = fs::equivalent(found->second.first, program); same && *same) {
                return found->second.second;
            }
        }
        std::string shim = mirror(fs::path::parent(program));
        fs::path::push(shim, name);
        return shim;
    }

private:
    /// Whether `program` is a file the system can run as a program: on Windows one whose name ends
    /// in `.exe`, as the shims' do.
    static bool runs(std::string_view program) {
        auto status = fs::status(program);
        if(!status || status->type != fs::FileType::Regular) {
            return false;
        }
#ifdef CATTER_WINDOWS
        auto extension = fs::path::extension(program);
        return extension.size() == 4 && sys::env_name_equal(extension, ".exe");
#else
        return (status->permissions & 0111) != 0;
#endif
    }

    /// Links catter-proxy as `name` in `directory`, and returns the link: on Windows a hard link of
    /// the launcher, which runs under the name it is started by; elsewhere a symlink to
    /// catter-proxy, which learns the name it is started by from the system.
    std::string link(std::string_view name, std::string_view directory) const {
        std::string path(directory);
        fs::path::push(path, name);
#ifdef CATTER_WINDOWS
        auto error = fs::create_hard_link(launcher(), path);
#else
        auto error = fs::create_symlink(program_path(PROXY), path);
#endif
        if(error) {
            throw cpptrace::runtime_error(
                std::format("Failed to make the shim {}: {}", path, error.message()));
        }
        return path;
    }

    /// A new directory among the shims, whose shims stand in for the programs of `directory`.
    std::string standing_for(std::string_view directory) {
        std::string shims = dir;
        fs::path::push(shims, std::to_string(++count));
        std::string named = shims;
        fs::path::push(named, SHIM_DIRECTORY_FILE);
        std::error_code error;
        if(auto created = fs::create_directory(shims); !created) {
            error = created.error();
        } else {
            error = fs::write_file(named, directory);
        }
        if(error) {
            throw cpptrace::runtime_error(
                std::format("Failed to make the shims of {}: {}", directory, error.message()));
        }
        return shims;
    }

    /// The directory that stands for `directory` among the shims, made the first time.
    std::string mirror(std::string_view directory) {
        if(auto found = mirrors.find(directory); found != mirrors.end()) {
            return found->second;
        }
        std::string mirror = standing_for(directory);
        if(auto entries = fs::Directory::open(directory)) {
            for(const auto& entry: *entries) {
                if(!entry) {
                    break;
                }
                std::string program(directory);
                fs::path::push(program, entry->name);
                if(!runs(program)) {
                    continue;
                }
                if(known(entry->name)) {
                    link(entry->name, mirror);
                    continue;
                }
#ifndef CATTER_WINDOWS
                // Windows lets few make a symlink.
                std::string path = mirror;
                fs::path::push(path, entry->name);
                if(auto error = fs::create_symlink(program, path)) {
                    throw cpptrace::runtime_error(
                        std::format("Failed to link {} to {}: {}", path, program, error.message()));
                }
#endif
            }
        }
        return mirrors.emplace(directory, std::move(mirror)).first->second;
    }

private:
    std::string dir;

    /// The directories among the shims, numbered.
    std::size_t count = 0;

    /// The program PATH finds first under each name catter knows, and its shim.
    std::map<std::string, std::pair<std::string, std::string>, std::less<>> names;

    /// The directory of the shims of each entry of PATH, none for one without any.
    std::map<std::string, std::optional<std::string>, std::less<>> on_path;

    /// The directory that stands for each directory of a program a variable names by its path.
    std::map<std::string, std::string, std::less<>> mirrors;
};

/// `value` of a variable that names a program to run, perhaps with arguments after it, which
/// runs the program's shim in its place: the shim of the program an absolute path names. A name is
/// looked for on PATH, where each directory has its shims before it; a relative path, which a build
/// tool takes from each directory it runs in, is left as it is.
std::string through_shim(std::string_view value, Shims& shims) {
    const auto end = value.find_first_of(" \t");
    const auto program = value.substr(0, end);
    if(!fs::path::is_absolute(program) || !known(fs::path::filename(program)) ||
       !fs::is_file(program)) {
        return std::string(value);
    }
    auto shim = shims.shim_of(program);
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
/// process's, with the shims of each directory on PATH before it, the programs variables such as
/// CC name by their paths run through theirs, and CMake running each compile and link through the
/// launcher, unless the build names a launcher of its own.
std::vector<async::process::env_var> environment(Shims& shims, std::string_view pipe) {
    auto named = [](std::string_view name) {
        return [name](std::string_view other) {
            return sys::env_name_equal(name, other);
        };
    };
    auto variables = sys::environment();
    auto path = ranges::find_if(variables, named("PATH"), &sys::EnvVar::name);
    if(path == variables.end()) {
        throw cpptrace::runtime_error(
            "The wrapper backend puts its shims on the build's PATH, which is not set: set it");
    }
    if(ranges::any_of(variables, named(env::SHIMS), &sys::EnvVar::name)) {
        throw cpptrace::runtime_error(
            "catter runs in a build under the wrapper backend, whose shims would stand in for this session's: run it outside that build, or with --capture hook");
    }
    std::vector<std::string> directories;
    for(auto directory: sys::split_paths(path->value)) {
        if(auto found = shims.add_found(directory)) {
            directories.push_back(std::move(*found));
        }
        directories.emplace_back(directory);
    }
    std::string path_list;
    if(auto error = sys::join_paths(path_list, directories)) {
        throw cpptrace::runtime_error(std::format(
            "catter cannot put its shims {} on PATH, which splits paths at what that path holds: set CATTER_HOME to a directory whose path has none",
            shims.directory()));
    }

    std::vector<async::process::env_var> env;
    for(auto& [name, value]: variables) {
        // The build command has no parent; the session's variables come last.
        if(named("PATH")(name) || ranges::any_of(env::VARIABLES, named(name))) {
            continue;
        }
        if(ranges::any_of(PROGRAM_VARIABLES, named(name))) {
            value = through_shim(value, shims);
        }
        env.push_back({.name = std::move(name), .value = std::move(value)});
    }

    env.push_back({.name = "PATH", .value = std::move(path_list)});
    env.push_back({.name = env::IPC, .value = std::string(pipe)});
    env.push_back({.name = env::SHIMS, .value = std::string(shims.directory())});

#ifdef CATTER_WINDOWS
    for(auto name: MSBUILD_PATHS) {
        if(ranges::none_of(env, named(name), &async::process::env_var::name)) {
            env.push_back({.name = std::string(name), .value = std::string(shims.directory())});
        }
    }
#endif
    for(auto language: CMAKE_LANGUAGES) {
        for(auto kind: {"COMPILER", "LINKER"}) {
            auto name = std::format("CMAKE_{}_{}_LAUNCHER", language, kind);
            if(ranges::none_of(env, named(name), &async::process::env_var::name)) {
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

    bool follows_decisions() const noexcept override {
        return true;
    }

    async::task<async::process::exit_status> run(const Build& build,
                                                 Session& session) const override {
        ipc::Server server(session);
        // A build tool that writes the shims' paths down writes them as they are spelled.
        auto directory = data_path(std::format("shims-{}", sys::pid()));
        fs::path::normalize(directory);
        Shims shims(std::move(directory));
        async::process::options launch{
            .program = shims.launcher(),
            .args = build.command,
            .env = environment(shims, server.pipe()),
            .cwd = build.cwd,
        };
        co_return co_await server.serve([launch = std::move(launch)]() mutable {
            return run_process(std::move(launch), {.stand_in = true});
        });
    }
};

}  // namespace

const Backend& wrapper_backend() noexcept {
    const static WrapperBackend backend;
    return backend;
}

}  // namespace catter
