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
#include <system_error>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <kota/sys/env.h>
#include <kota/sys/process.h>

#include "hook/unix/command.h"
#include "hook/unix/crossplat.h"
#include "hook/unix/debug.h"
#include "hook/unix/env_sanitizer.h"
#include "hook/unix/environment.h"
#include "hook/unix/error.h"
#include "hook/unix/session.h"

namespace catter::hook {
namespace {

ArgvRef argv_span(const char* const argv[]) noexcept {
    if(argv == nullptr) {
        return {};
    }

    std::size_t argc = 0;
    while(argv[argc] != nullptr) {
        ++argc;
    }
    return std::span(argv, argc);
}

void require_path_arg(const char* value, std::string_view name) {
    if(value == nullptr) {
        throw PayloadError(EFAULT, std::format("Argument `{}` must not be null", name));
    }
}

void require_va_list(va_list* ap) {
    if(ap == nullptr) {
        throw PayloadError(EFAULT, "va_list must not be null");
    }
}

std::vector<const char*> collect_variadic_argv(const char* first_arg, va_list* ap) {
    std::vector<const char*> argv;
    if(first_arg != nullptr) {
        argv.push_back(first_arg);
        while(auto* arg = va_arg(*ap, const char*)) {
            argv.push_back(arg);
        }
    }
    argv.push_back(nullptr);
    return argv;
}

char** collect_variadic_envp(va_list* ap) {
    return va_arg(*ap, char**);
}

/// The program `file` names, looked for as `search` says, by execvp's rules;
/// `{.directories = {""}}` looks in the current directory alone, from which
/// execve runs a path without a `/`.
std::string resolve_program(const char* file, const kota::sys::ProgramSearch& search) {
    std::string path;
    if(auto error = kota::sys::find_program(path, file, search)) {
        throw PayloadError(error.value(), std::format("failed to resolve executable: {}", file));
    }
    return path;
}

std::string resolve_from_search_path(const char* file, std::string_view search_path) {
    kota::sys::ProgramSearch search;
    for(auto directory: kota::sys::split_paths(search_path)) {
        search.directories.emplace_back(directory);
    }
    return resolve_program(file, search);
}

/// The program `file` names, looked for in the PATH of `envp`, or in
/// confstr(_CS_PATH)'s directories without one, as execvp looks.
std::string resolve_from_path(const char* file, const char* const envp[]) {
    // Read here: kota::sys reads the environment under a lock, which a child
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
    /// None the system knows of: execve fails with ENOEXEC, or a format
    /// registered with binfmt_misc starts it.
    Unknown,
    /// A script whose interpreter is not there: execve fails with ENOENT.
    MissingInterpreter,
};

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
        std::string interpreter(text.substr(0, end));
        return ::access(interpreter.c_str(), X_OK) == 0 ? Format::Program
                                                        : Format::MissingInterpreter;
    }
#ifdef CATTER_LINUX
    if(text.starts_with("\x7f" "ELF")) {
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

/// The system judges a file catter-proxy would run otherwise: it fails, as
/// without catter, or it starts the file itself, uncaptured.
bool for_the_system(const char* executable, bool shell_fallback) noexcept {
    auto format = format_of(executable);
    return format == Format::MissingInterpreter || (format == Format::Unknown && !shell_fallback);
}

/// A program that is not there or not executable fails to start as it would
/// without catter: a build may expect that (a PATH search tries every entry).
void report_failure([[maybe_unused]] std::string_view name,
                    [[maybe_unused]] const PayloadError& err) {
    if(err.code() == ENOENT || err.code() == EACCES || err.code() == ENOTDIR) {
        INFO("{} failed: {}", name, err.what());
    } else {
        ERROR("{} failed: {}", name, err.what());
    }
}
}  // namespace
}  // namespace catter::hook

// clang-format 23 splits `try BODY catch` apart.
// clang-format off
#define CATTER_EXEC_BOUNDARY(NAME, BODY)                                                           \
    try BODY catch(const catter::hook::PayloadError& err) {                                        \
        report_failure(NAME, err);                                                                 \
        errno = err.code();                                                                        \
        return -1;                                                                                 \
    }                                                                                              \
    catch(const std::exception& err) {                                                             \
        ERROR("{} failed with unexpected exception: {}", NAME, err.what());                        \
        errno = ENOSYS;                                                                            \
        return -1;                                                                                 \
    }                                                                                              \
    catch(...) {                                                                                   \
        ERROR("{} failed with unknown exception", NAME);                                           \
        errno = ENOSYS;                                                                            \
        return -1;                                                                                 \
    }

#define CATTER_SPAWN_BOUNDARY(NAME, BODY)                                                          \
    try BODY catch(const catter::hook::PayloadError& err) {                                        \
        report_failure(NAME, err);                                                                 \
        errno = err.code();                                                                        \
        return err.code();                                                                         \
    }                                                                                              \
    catch(const std::exception& err) {                                                             \
        ERROR("{} failed with unexpected exception: {}", NAME, err.what());                        \
        errno = ENOSYS;                                                                            \
        return ENOSYS;                                                                             \
    }                                                                                              \
    catch(...) {                                                                                   \
        ERROR("{} failed with unknown exception", NAME);                                           \
        errno = ENOSYS;                                                                            \
        return ENOSYS;                                                                             \
    }
// clang-format on

namespace catter::hook {

ExecveFn* resolve_execve() {
#ifdef CATTER_MAC
    const auto fp = &::execve;
#endif
#ifdef CATTER_LINUX
    const auto fp = dynamic_linker<ExecveFn*>("execve");
#endif
    if(fp == nullptr) {
        throw PayloadError(ENOSYS, "hook function \"execve\" not found");
    }
    return fp;
}

PosixSpawnFn* resolve_posix_spawn() {
#ifdef CATTER_MAC
    const auto fp = &::posix_spawn;
#endif
#ifdef CATTER_LINUX
    const auto fp = dynamic_linker<PosixSpawnFn*>("posix_spawn");
#endif
    if(fp == nullptr) {
        throw PayloadError(ENOSYS, "hook function \"posix_spawn\" not found");
    }
    return fp;
}

void Executor::init(const char* const envp[]) noexcept {
    m_session = Session::make(envp);

    try {
        m_execve = resolve_execve();
        m_posix_spawn = resolve_posix_spawn();
    } catch(const std::exception& err) {
        ERROR("failed to initialize hook target: {}", err.what());
        m_execve = nullptr;
        m_posix_spawn = nullptr;
    } catch(...) {
        ERROR("failed to initialize hook target with unknown exception");
        m_execve = nullptr;
        m_posix_spawn = nullptr;
    }
}

void Executor::init(Session session, ExecveFn* execve, PosixSpawnFn* posix_spawn) noexcept {
    m_session = session;
    m_execve = execve;
    m_posix_spawn = posix_spawn;
}

int Executor::execv(const char* path, char* const argv[]) noexcept {
    CATTER_EXEC_BOUNDARY("execv", {
        require_path_arg(path, "path");
        return this->run_execve(resolve_program(path, {.directories = {""}}).c_str(),
                                argv,
                                environment(),
                                false);
    });
}

int Executor::execve(const char* path, char* const argv[], char* const envp[]) noexcept {
    CATTER_EXEC_BOUNDARY("execve", {
        require_path_arg(path, "path");
        return this->run_execve(resolve_program(path, {.directories = {""}}).c_str(),
                                argv,
                                envp,
                                false);
    });
}

int Executor::execvp(const char* file, char* const argv[]) noexcept {
    CATTER_EXEC_BOUNDARY("execvp", {
        require_path_arg(file, "file");
        auto envp = environment();
        return this->run_execve(resolve_from_path(file, envp).c_str(), argv, envp, true);
    });
}

int Executor::execvpe(const char* file, char* const argv[], char* const envp[]) noexcept {
    CATTER_EXEC_BOUNDARY("execvpe", {
        require_path_arg(file, "file");
        return this->run_execve(resolve_from_path(file, environment()).c_str(), argv, envp, true);
    });
}

int Executor::execl(const char* path, const char* arg, va_list* ap) noexcept {
    CATTER_EXEC_BOUNDARY("execl", {
        require_path_arg(path, "path");
        require_va_list(ap);
        auto argv = collect_variadic_argv(arg, ap);
        return this->run_execve(resolve_program(path, {.directories = {""}}).c_str(),
                                argv.data(),
                                environment(),
                                false);
    });
}

int Executor::execle(const char* path, const char* arg, va_list* ap) noexcept {
    CATTER_EXEC_BOUNDARY("execle", {
        require_path_arg(path, "path");
        require_va_list(ap);
        auto argv = collect_variadic_argv(arg, ap);
        auto envp = collect_variadic_envp(ap);
        return this->run_execve(resolve_program(path, {.directories = {""}}).c_str(),
                                argv.data(),
                                envp,
                                false);
    });
}

int Executor::execlp(const char* file, const char* arg, va_list* ap) noexcept {
    CATTER_EXEC_BOUNDARY("execlp", {
        require_path_arg(file, "file");
        require_va_list(ap);
        auto argv = collect_variadic_argv(arg, ap);
        auto envp = environment();
        return this->run_execve(resolve_from_path(file, envp).c_str(), argv.data(), envp, true);
    });
}

int Executor::execvP(const char* file,
                     const char* search_path,
                     char* const argv[],
                     char* const envp[]) noexcept {
    CATTER_EXEC_BOUNDARY("execvP", {
        require_path_arg(file, "file");
        require_path_arg(search_path, "search_path");
        return this->run_execve(resolve_from_search_path(file, search_path).c_str(),
                                argv,
                                envp,
                                true);
    });
}

int Executor::exect(const char* path, char* const argv[], char* const envp[]) noexcept {
    CATTER_EXEC_BOUNDARY("exect", {
        require_path_arg(path, "path");
        return this->run_execve(resolve_program(path, {.directories = {""}}).c_str(),
                                argv,
                                envp,
                                false);
    });
}

int Executor::posix_spawn(pid_t* pid,
                          const char* path,
                          const posix_spawn_file_actions_t* file_actions,
                          const posix_spawnattr_t* attrp,
                          char* const argv[],
                          char* const envp[]) noexcept {
    CATTER_SPAWN_BOUNDARY("posix_spawn", {
        require_path_arg(path, "path");
        return this->run_posix_spawn(pid,
                                     resolve_program(path, {.directories = {""}}).c_str(),
                                     file_actions,
                                     attrp,
                                     argv,
                                     envp,
                                     false);
    });
}

int Executor::posix_spawnp(pid_t* pid,
                           const char* file,
                           const posix_spawn_file_actions_t* file_actions,
                           const posix_spawnattr_t* attrp,
                           char* const argv[],
                           char* const envp[]) noexcept {
    CATTER_SPAWN_BOUNDARY("posix_spawnp", {
        require_path_arg(file, "file");
        return this->run_posix_spawn(pid,
                                     resolve_from_path(file, environment()).c_str(),
                                     file_actions,
                                     attrp,
                                     argv,
                                     envp,
                                     POSIX_SPAWNP_SHELL_FALLBACK);
    });
}

int Executor::run_execve(const char* executable,
                         const char* const argv[],
                         char* const envp[],
                         bool shell_fallback) {
    if(this->m_execve == nullptr) {
        throw PayloadError(ENOSYS, "hook function \"execve\" not initialized");
    }
    if(for_the_system(executable, shell_fallback)) {
        return m_execve(executable, const_cast<char* const*>(argv), envp);
    }

    auto clean_env = sanitize_environment(envp);
    auto args = argv_span(argv);
    if(!m_session.is_valid()) {
        auto command =
            build_error_command(m_session,
                                "invalid environment of hook library, lost required value",
                                executable,
                                args);

        return m_execve(command.path.c_str(), command.c_argv().data(), clean_env.data());
    }

    auto command = build_proxy_command(m_session, executable, args);
    auto c_argv = command.c_argv();

    INFO("execve called with path: {}, argv[0]: {}",
         command.path,
         c_argv[0] == nullptr ? "" : c_argv[0]);

    return m_execve(command.path.c_str(), c_argv.data(), clean_env.data());
}

int Executor::run_posix_spawn(pid_t* pid,
                              const char* executable,
                              const posix_spawn_file_actions_t* file_actions,
                              const posix_spawnattr_t* attrp,
                              const char* const argv[],
                              char* const envp[],
                              bool shell_fallback) {
    if(m_posix_spawn == nullptr) {
        throw PayloadError(ENOSYS, "hook function \"posix_spawn\" not initialized");
    }
    if(for_the_system(executable, shell_fallback)) {
        return m_posix_spawn(pid,
                             executable,
                             file_actions,
                             attrp,
                             const_cast<char* const*>(argv),
                             envp);
    }
    auto clean_env = sanitize_environment(envp);
    auto args = argv_span(argv);
    if(!m_session.is_valid()) {
        auto command =
            build_error_command(m_session,
                                "invalid environment of hook library, lost required value",
                                executable,
                                args);

        return m_posix_spawn(pid,
                             command.path.c_str(),
                             file_actions,
                             attrp,
                             command.c_argv().data(),
                             clean_env.data());
    }

    auto command = build_proxy_command(m_session, executable, args);
    auto c_argv = command.c_argv();

    INFO("posix_spawn called with path: {}, argv[0]: {}",
         command.path,
         c_argv[0] == nullptr ? "" : c_argv[0]);
    return m_posix_spawn(pid,
                         command.path.c_str(),
                         file_actions,
                         attrp,
                         c_argv.data(),
                         clean_env.data());
}

}  // namespace catter::hook

#undef CATTER_EXEC_BOUNDARY
#undef CATTER_SPAWN_BOUNDARY
