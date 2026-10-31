#include "hook/unix/executor.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <exception>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <kotatsu/sys/env.h>
#include <kotatsu/sys/process.h>

#ifdef CATTER_LINUX
#include <optional>
#include <sys/auxv.h>
#include <kotatsu/fs/status.h>
#endif

#include "hook/unix/debug.h"
#include "hook/unix/environment.h"
#include "support/hook.h"
#include "support/messages.h"

namespace catter::hook {

namespace {

/// Why a call fails, as the C library would fail it: `code`, as errno.
struct Error : std::exception {
    Error(int code, std::string message) : code(code), message(std::move(message)) {}

    const char* what() const noexcept override {
        return message.c_str();
    }

    int code;

    std::string message;
};

/// The program `file` names, looked for as `search` says, by execvp's rules;
/// `{.directories = {""}}` looks in the current directory alone, from which
/// execve runs a path without a `/`.
std::string resolve_program(const char* file, const kotatsu::sys::ProgramSearch& search) {
    std::string path;
    if(auto error = kotatsu::sys::find_program(path, file, search)) {
        throw Error(error.value(), std::format("failed to resolve executable: {}", file));
    }
    return path;
}

std::string resolve_from_search_path(const char* file, std::string_view search_path) {
    kotatsu::sys::ProgramSearch search;
    for(auto directory: kotatsu::sys::split_paths(search_path)) {
        search.directories.emplace_back(directory);
    }
    return resolve_program(file, search);
}

/// The program `file` names, looked for in the PATH of `envp`, or in
/// confstr(_CS_PATH)'s directories without one, as execvp looks.
std::string resolve_from_path(const char* file, const char* const envp[]) {
    if(file == nullptr) {
        throw Error(EFAULT, "The file of the program is null");
    }
    // Read here: kotatsu::sys reads the environment under a lock, which a child
    // forked while another thread held it would wait on forever.
    if(auto path = envp == nullptr ? nullptr : get_env_value(envp, "PATH")) {
        return resolve_from_search_path(file, path);
    }
    std::string defaults(::confstr(_CS_PATH, nullptr, 0), '\0');
    ::confstr(_CS_PATH, defaults.data(), defaults.size());
    defaults.pop_back();
    return resolve_from_search_path(file, defaults);
}

/// How the system takes a file it is asked to run, where catter-proxy would
/// run it otherwise.
enum class Format : std::uint8_t {
    /// One the system starts itself, or that this process cannot read, as a
    /// program only executable is: catter-proxy starts it.
    Program,
    /// None the system knows of: execve fails with ENOEXEC.
    Unknown,
    /// A script whose interpreter is not there: execve fails with ENOENT.
    MissingInterpreter,
};

#ifdef CATTER_LINUX
int hex_digit(char c) noexcept {
    if(c >= '0' && c <= '9') {
        return c - '0';
    }
    if(c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

/// Whether the registration of binfmt_misc that `text` shows, as its file
/// under /proc/sys/fs/binfmt_misc does, starts the file `path`, whose first
/// bytes are `head`.
bool registration_starts(std::string_view text,
                         std::string_view path,
                         std::string_view head) noexcept {
    if(!text.starts_with("enabled\n")) {
        return false;
    }
    std::size_t offset = 0;
    std::string_view magic;
    std::string_view mask;
    std::string_view extension;
    while(!text.empty()) {
        auto line = text.substr(0, text.find('\n'));
        text.remove_prefix(std::min(line.size() + 1, text.size()));
        if(line.starts_with("offset ")) {
            offset = 0;
            for(char c: line.substr(7)) {
                offset = offset * 10 + static_cast<std::size_t>(c - '0');
            }
        } else if(line.starts_with("magic ")) {
            magic = line.substr(6);
        } else if(line.starts_with("mask ")) {
            mask = line.substr(5);
        } else if(line.starts_with("extension .")) {
            extension = line.substr(11);
        }
    }
    if(!extension.empty()) {
        auto dot = path.rfind('.');
        return dot != std::string_view::npos && path.substr(dot + 1) == extension;
    }
    if(magic.empty() || magic.size() % 2 != 0 || offset + magic.size() / 2 > head.size()) {
        return false;
    }
    for(std::size_t i = 0; i < magic.size() / 2; i += 1) {
        int byte = hex_digit(magic[2 * i]) * 16 + hex_digit(magic[2 * i + 1]);
        int bits = mask.size() == magic.size()
                       ? hex_digit(mask[2 * i]) * 16 + hex_digit(mask[2 * i + 1])
                       : 0xff;
        if((static_cast<unsigned char>(head[offset + i]) & bits) != (byte & bits)) {
            return false;
        }
    }
    return true;
}

/// Whether a format registered with binfmt_misc starts the file `path`,
/// whose first bytes are `head`, as WSL starts Windows programs and Wine
/// those it runs: the kernel starts it, as catter-proxy does then.
bool registered_format(std::string_view path, std::string_view head) noexcept {
    DIR* registrations = ::opendir("/proc/sys/fs/binfmt_misc");
    if(registrations == nullptr) {
        return false;
    }
    bool found = false;
    while(!found) {
        auto* entry = ::readdir(registrations);
        if(entry == nullptr) {
            break;
        }
        std::string_view name(entry->d_name);
        if(name.starts_with('.') || name == "register" || name == "status") {
            continue;
        }
        int fd = ::openat(::dirfd(registrations), entry->d_name, O_RDONLY | O_CLOEXEC);
        if(fd < 0) {
            continue;
        }
        std::array<char, 1024> text;
        auto size = ::read(fd, text.data(), text.size());
        ::close(fd);
        found = size > 0 &&
                registration_starts(std::string_view(text.data(), static_cast<std::size_t>(size)),
                                    path,
                                    head);
    }
    ::closedir(registrations);
    return found;
}
#endif

Format format_of(const char* path) noexcept {
    int fd = ::open(path, O_RDONLY | O_CLOEXEC);
    if(fd < 0) {
        return Format::Program;
    }
    std::array<char, 256> head;
    auto size = ::read(fd, head.data(), head.size());
    ::close(fd);
    if(size < 0) {
        return Format::Program;
    }
    std::string_view text(head.data(), static_cast<std::size_t>(size));
    if(text.starts_with("#!")) {
        text.remove_prefix(2);
        text.remove_prefix(std::min(text.find_first_not_of(" \t"), text.size()));
        auto end = text.find_first_of(" \t\n");
        if(end == std::string_view::npos) {
            return Format::Program;
        }
        if(end == 0) {
            // No interpreter: no format the system knows of.
            return Format::Unknown;
        }
        const auto interpreter = text.data();
        head[static_cast<std::size_t>(interpreter - head.data()) + end] = '\0';
        return ::access(interpreter, X_OK) == 0 ? Format::Program : Format::MissingInterpreter;
    }
#ifdef CATTER_LINUX
    if(text.starts_with("\x7f" "ELF") || registered_format(path, text)) {
        return Format::Program;
    }
#else
    if(text.size() >= 4) {
        std::uint32_t magic;
        std::memcpy(&magic, text.data(), sizeof(magic));
        // Mach-O, 32 and 64 bits, and universal, in either byte order.
        for(std::uint32_t known: {0xfeedfaceU, 0xfeedfacfU, 0xcafebabeU, 0xcafebabfU}) {
            if(magic == known || magic == __builtin_bswap32(known)) {
                return Format::Program;
            }
        }
    }
#endif
    return Format::Unknown;
}

/// Whether posix_spawnp runs a file of no executable format with /bin/sh, as
/// execvp does: macOS's does, glibc's no longer.
#ifdef CATTER_MAC
constexpr bool POSIX_SPAWNP_SHELL_FALLBACK = true;
#else
constexpr bool POSIX_SPAWNP_SHELL_FALLBACK = false;
#endif

/// The system judges a file catter-proxy would run otherwise, and fails it as
/// without catter.
bool for_the_system(const char* executable, bool shell_fallback) noexcept {
    auto format = format_of(executable);
    return format == Format::MissingInterpreter || (format == Format::Unknown && !shell_fallback);
}

/// A program that is not there or not executable fails to start as it would
/// without catter: a build may expect that (a PATH search tries every entry).
void report_failure([[maybe_unused]] std::string_view name, [[maybe_unused]] const Error& err) {
    if(err.code == ENOENT || err.code == EACCES || err.code == ENOTDIR) {
        INFO("{} failed: {}", name, err.what());
    } else {
        ERROR("{} failed: {}", name, err.what());
    }
}

/// Runs `call`, the hook of `name`, which no exception leaves: one fails the
/// call as the C library fails it, with -1 and errno, or for a spawn with the
/// error number.
template <typename Call>
int guarded(std::string_view name, bool spawn, Call&& call) noexcept {
    int code = ENOSYS;
    try {
        return call();
    } catch(const Error& error) {
        report_failure(name, error);
        code = error.code;
    } catch(const std::exception& error) {
        ERROR("{} failed with unexpected exception: {}", name, error.what());
    } catch(...) {
        ERROR("{} failed with unknown exception", name);
    }
    errno = code;
    return spawn ? code : -1;
}

/// The program execve runs for `path`: a path, or a name in the current
/// directory.
std::string path_of(const char* path) {
    if(path == nullptr) {
        throw Error(EFAULT, "The path of the program is null");
    }
    return resolve_program(path, {.directories = {""}});
}

/// The arguments of execl and its kind, which end with a null.
std::vector<const char*> variadic_argv(const char* first, va_list* ap) {
    std::vector<const char*> argv;
    if(first != nullptr) {
        argv.push_back(first);
        while(auto* arg = va_arg(*ap, const char*)) {
            argv.push_back(arg);
        }
    }
    argv.push_back(nullptr);
    return argv;
}

/// What starts catter-proxy in place of `executable` run with `argv`.
std::vector<char*> proxy_argv(std::string& proxy,
                              const char* executable,
                              const char* const argv[]) {
    std::vector<char*> args = {proxy.data(), const_cast<char*>(executable)};
    for(auto arg = argv; arg != nullptr && *arg != nullptr; ++arg) {
        args.push_back(const_cast<char*>(*arg));
    }
    args.push_back(nullptr);
    return args;
}

#ifdef CATTER_LINUX
/// The program to start this process again with: the file its exec named, but
/// for a script or a format binfmt_misc runs, the interpreter, which runs, and
/// for a name of this process's own (`/proc/self/exe`, `/dev/fd/3` of
/// fexecve), which would name catter-proxy then. None for a program that has
/// no name outside its process, as a deleted file or a memfd has.
std::optional<std::string> restartable_program() {
    std::string image;
    if(kotatsu::sys::executable_path(image) || !kotatsu::fs::is_file(image)) {
        return std::nullopt;
    }
    const std::string_view named = reinterpret_cast<const char*>(::getauxval(AT_EXECFN));
    if(named.starts_with("/proc/") || named.starts_with("/dev/fd/")) {
        return image;
    }
    if(auto same = kotatsu::fs::equivalent(named, image); same && *same) {
        return std::string(named);
    }
    return image;
}
#endif

}  // namespace

void Executor::init(const char* const envp[],
                    ExecveFn* execve,
                    PosixSpawnFn* posix_spawn) noexcept {
    if(auto value = get_env_value(envp, env::PROXY)) {
        proxy = value;
    }
    for(auto name: {env::IPC, env::COMMAND_ID}) {
        if(auto entry = get_env_entry(envp, name)) {
            session.emplace_back(entry);
        }
    }
    original_execve = execve;
    original_posix_spawn = posix_spawn;
}

int Executor::execv(const char* path, char* const argv[]) noexcept {
    return guarded("execv", false, [&] {
        return run_execve(path_of(path).c_str(), argv, environment(), false);
    });
}

int Executor::execve(const char* path, char* const argv[], char* const envp[]) noexcept {
    return guarded("execve", false, [&] {
        return run_execve(path_of(path).c_str(), argv, envp, false);
    });
}

int Executor::execvp(const char* file, char* const argv[]) noexcept {
    return guarded("execvp", false, [&] {
        auto envp = environment();
        return run_execve(resolve_from_path(file, envp).c_str(), argv, envp, true);
    });
}

int Executor::execvpe(const char* file, char* const argv[], char* const envp[]) noexcept {
    return guarded("execvpe", false, [&] {
        return run_execve(resolve_from_path(file, environment()).c_str(), argv, envp, true);
    });
}

int Executor::execl(const char* path, const char* arg, va_list* ap) noexcept {
    return guarded("execl", false, [&] {
        auto argv = variadic_argv(arg, ap);
        return run_execve(path_of(path).c_str(), argv.data(), environment(), false);
    });
}

int Executor::execle(const char* path, const char* arg, va_list* ap) noexcept {
    return guarded("execle", false, [&] {
        auto argv = variadic_argv(arg, ap);
        auto envp = va_arg(*ap, char**);
        return run_execve(path_of(path).c_str(), argv.data(), envp, false);
    });
}

int Executor::execlp(const char* file, const char* arg, va_list* ap) noexcept {
    return guarded("execlp", false, [&] {
        auto argv = variadic_argv(arg, ap);
        auto envp = environment();
        return run_execve(resolve_from_path(file, envp).c_str(), argv.data(), envp, true);
    });
}

int Executor::execvP(const char* file,
                     const char* search_path,
                     char* const argv[],
                     char* const envp[]) noexcept {
    return guarded("execvP", false, [&] {
        if(file == nullptr || search_path == nullptr) {
            throw Error(EFAULT, "The file of the program or its search path is null");
        }
        return run_execve(resolve_from_search_path(file, search_path).c_str(), argv, envp, true);
    });
}

int Executor::posix_spawn(pid_t* pid,
                          const char* path,
                          const posix_spawn_file_actions_t* file_actions,
                          const posix_spawnattr_t* attrp,
                          char* const argv[],
                          char* const envp[]) noexcept {
    return guarded("posix_spawn", true, [&] {
        return run_posix_spawn(pid, path_of(path).c_str(), file_actions, attrp, argv, envp, false);
    });
}

int Executor::posix_spawnp(pid_t* pid,
                           const char* file,
                           const posix_spawn_file_actions_t* file_actions,
                           const posix_spawnattr_t* attrp,
                           char* const argv[],
                           char* const envp[]) noexcept {
    return guarded("posix_spawnp", true, [&] {
        return run_posix_spawn(pid,
                               resolve_from_path(file, environment()).c_str(),
                               file_actions,
                               attrp,
                               argv,
                               envp,
                               POSIX_SPAWNP_SHELL_FALLBACK);
    });
}

#ifdef CATTER_LINUX
void Executor::restart_undecided(int argc, char* argv[]) noexcept {
    if(proxy.empty()) {
        return;
    }
    const char* decided = get_env_value(environment(), env::DECIDED);
    const bool started =
        decided != nullptr &&
        is_decided(decided, ::getppid(), std::span(argv, static_cast<std::size_t>(argc)));
    ::unsetenv(env::DECIDED);
    // ld.so run as the program, as `ld.so <program>` does, takes its own
    // arguments out of argv: the hook cannot tell its exec, nor start it again.
    // valgrind runs the program inside its own process, which no exec of the
    // program's started, and which a restart would leave; it preloads its core
    // into the program.
    const char* preload = get_env_value(environment(), PRELOAD);
    if(started || ::getauxval(AT_BASE) == 0 ||
       (preload != nullptr && std::string_view(preload).contains("/vgpreload_core-"))) {
        return;
    }
    guarded("restart", false, [&] {
        const auto program = restartable_program();
        if(!program) {
            return 0;
        }
        INFO("{} was started undecided: it restarts through {}", *program, proxy);
        auto variables = session;
        variables.push_back(std::format("{}=1", env::LATE));
        auto env = proxy_environment(environment(), variables);
        return original_execve(proxy.c_str(),
                               proxy_argv(proxy, program->c_str(), argv).data(),
                               env.entries.data());
    });
}
#endif

int Executor::run_execve(const char* executable,
                         const char* const argv[],
                         char* const envp[],
                         bool shell_fallback) {
    if(original_execve == nullptr) {
        throw Error(ENOSYS, "The C library has no execve");
    }
    if(proxy.empty() || for_the_system(executable, shell_fallback)) {
        return original_execve(executable, const_cast<char* const*>(argv), envp);
    }
    INFO("{} runs through {}", executable, proxy);
    auto env = proxy_environment(envp, session);
    return original_execve(proxy.c_str(),
                           proxy_argv(proxy, executable, argv).data(),
                           env.entries.data());
}

int Executor::run_posix_spawn(pid_t* pid,
                              const char* executable,
                              const posix_spawn_file_actions_t* file_actions,
                              const posix_spawnattr_t* attrp,
                              const char* const argv[],
                              char* const envp[],
                              bool shell_fallback) {
    if(original_posix_spawn == nullptr) {
        throw Error(ENOSYS, "The C library has no posix_spawn");
    }
    if(proxy.empty() || for_the_system(executable, shell_fallback)) {
        return original_posix_spawn(pid,
                                    executable,
                                    file_actions,
                                    attrp,
                                    const_cast<char* const*>(argv),
                                    envp);
    }
    INFO("{} starts through {}", executable, proxy);
    auto env = proxy_environment(envp, session);
    return original_posix_spawn(pid,
                                proxy.c_str(),
                                file_actions,
                                attrp,
                                proxy_argv(proxy, executable, argv).data(),
                                env.entries.data());
}

}  // namespace catter::hook
