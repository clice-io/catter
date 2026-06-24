#include "executor.h"

#include <cerrno>
#include <cstdarg>
#include <exception>
#include <filesystem>
#include <format>
#include <span>
#include <string_view>
#include <vector>

#include "command.h"
#include "crossplat.h"
#include "debug.h"
#include "env_sanitizer.h"
#include "environment.h"
#include "error.h"
#include "session.h"
#include "shared/resolver.h"

namespace {

catter::ArgvRef argv_span(const char* const argv[]) noexcept {
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
        throw catter::PayloadError(EFAULT, std::format("{} must not be null", name));
    }
}

void require_va_list(va_list* ap) {
    if(ap == nullptr) {
        throw catter::PayloadError(EFAULT, "va_list must not be null");
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

std::filesystem::path resolve_path_like(const char* path) {
    auto resolved = catter::hook::shared::resolver::resolve_path_like(path);
    if(!resolved.has_value()) {
        throw catter::PayloadError(resolved.error(),
                                   std::format("failed to resolve executable: {}", path));
    }
    return *resolved;
}

std::filesystem::path resolve_from_path(const char* file, const char* const envp[]) {
    auto path_env = envp == nullptr ? nullptr : catter::env::get_env_value(envp, "PATH");
    auto resolved = catter::hook::shared::resolver::resolve_from_path_env(file, path_env);
    if(!resolved.has_value()) {
        throw catter::PayloadError(resolved.error(),
                                   std::format("failed to resolve executable from PATH: {}", file));
    }
    return *resolved;
}

std::filesystem::path resolve_from_search_path(const char* file, const char* search_path) {
    auto resolved = catter::hook::shared::resolver::resolve_from_search_path(file, search_path);
    if(!resolved.has_value()) {
        throw catter::PayloadError(
            resolved.error(),
            std::format("failed to resolve executable from search path: {}", file));
    }
    return *resolved;
}
}  // namespace

#define CATTER_EXEC_BOUNDARY(NAME, BODY)                                                           \
    try BODY catch(const catter::PayloadError& err) {                                              \
        ERROR("{} failed: {}", NAME, err.what());                                                  \
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
    try BODY catch(const catter::PayloadError& err) {                                              \
        ERROR("{} failed: {}", NAME, err.what());                                                  \
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

namespace catter {

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
        return this->run_execve(resolve_path_like(path).c_str(), argv, environment());
    });
}

int Executor::execve(const char* path, char* const argv[], char* const envp[]) noexcept {
    CATTER_EXEC_BOUNDARY("execve", {
        require_path_arg(path, "path");
        return this->run_execve(resolve_path_like(path).c_str(), argv, envp);
    });
}

int Executor::execvp(const char* file, char* const argv[]) noexcept {
    CATTER_EXEC_BOUNDARY("execvp", {
        require_path_arg(file, "file");
        auto envp = environment();
        return this->run_execve(resolve_from_path(file, envp).c_str(), argv, envp);
    });
}

int Executor::execvpe(const char* file, char* const argv[], char* const envp[]) noexcept {
    CATTER_EXEC_BOUNDARY("execvpe", {
        require_path_arg(file, "file");
        return this->run_execve(resolve_from_path(file, environment()).c_str(), argv, envp);
    });
}

int Executor::execl(const char* path, const char* arg, va_list* ap) noexcept {
    CATTER_EXEC_BOUNDARY("execl", {
        require_path_arg(path, "path");
        require_va_list(ap);
        auto argv = collect_variadic_argv(arg, ap);
        return this->run_execve(resolve_path_like(path).c_str(), argv.data(), environment());
    });
}

int Executor::execle(const char* path, const char* arg, va_list* ap) noexcept {
    CATTER_EXEC_BOUNDARY("execle", {
        require_path_arg(path, "path");
        require_va_list(ap);
        auto argv = collect_variadic_argv(arg, ap);
        auto envp = collect_variadic_envp(ap);
        return this->run_execve(resolve_path_like(path).c_str(), argv.data(), envp);
    });
}

int Executor::execlp(const char* file, const char* arg, va_list* ap) noexcept {
    CATTER_EXEC_BOUNDARY("execlp", {
        require_path_arg(file, "file");
        require_va_list(ap);
        auto argv = collect_variadic_argv(arg, ap);
        auto envp = environment();
        return this->run_execve(resolve_from_path(file, envp).c_str(), argv.data(), envp);
    });
}

int Executor::execvP(const char* file,
                     const char* search_path,
                     char* const argv[],
                     char* const envp[]) noexcept {
    CATTER_EXEC_BOUNDARY("execvP", {
        require_path_arg(file, "file");
        require_path_arg(search_path, "search_path");
        return run_execve(resolve_from_search_path(file, search_path).c_str(), argv, envp);
    });
}

int Executor::exect(const char* path, char* const argv[], char* const envp[]) noexcept {
    CATTER_EXEC_BOUNDARY("exect", {
        require_path_arg(path, "path");
        return this->run_execve(resolve_path_like(path).c_str(), argv, environment());
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
                                     resolve_path_like(path).c_str(),
                                     file_actions,
                                     attrp,
                                     argv,
                                     envp);
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
                                     resolve_from_path(file, envp).c_str(),
                                     file_actions,
                                     attrp,
                                     argv,
                                     envp);
    });
}

int Executor::run_execve(const char* executable, const char* const argv[], char* const envp[]) {
    if(this->m_execve == nullptr) {
        throw catter::PayloadError(ENOSYS, "hook function \"execve\" not initialized");
    }

    auto clean_env = catter::sanitize_environment(envp);
    auto args = argv_span(argv);
    if(!m_session.is_valid()) {
        auto command =
            catter::build_error_command(m_session,
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
                              char* const envp[]) {
    if(m_posix_spawn == nullptr) {
        throw catter::PayloadError(ENOSYS, "hook function \"posix_spawn\" not initialized");
    }
    auto clean_env = catter::sanitize_environment(envp);
    auto args = argv_span(argv);
    if(!m_session.is_valid()) {
        auto command =
            catter::build_error_command(m_session,
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

}  // namespace catter

#undef CATTER_EXEC_BOUNDARY
#undef CATTER_SPAWN_BOUNDARY
