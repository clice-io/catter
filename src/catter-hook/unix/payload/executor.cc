#include "executor.h"

#include <cerrno>
#include <exception>
#include <filesystem>
#include <format>
#include <span>
#include <string_view>

#include "command.h"
#include "crossplat.h"
#include "debug.h"
#include "env_guard.h"
#include "environment.h"
#include "error.h"
#include "session.h"
#include "shared/resolver.h"

namespace {

catter::ArgvRef argv_span(char* const* argv) noexcept {
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

catter::Command invalid_session_command(const catter::Session& session,
                                        const std::filesystem::path& executable,
                                        catter::ArgvRef argv) {
    return catter::build_error_command(session,
                                       "invalid environment of hook library, lost required value",
                                       executable,
                                       argv);
}

std::filesystem::path resolve_path_like(const char* path) {
    auto resolved = catter::hook::shared::resolver::resolve_path_like(path);
    if(!resolved.has_value()) {
        throw catter::PayloadError(resolved.error(),
                                   std::format("failed to resolve executable: {}", path));
    }
    return *resolved;
}

std::filesystem::path resolve_from_path(const char* file, char* const envp[]) {
    auto path_env = envp == nullptr
                        ? nullptr
                        : catter::env::get_env_value(const_cast<const char**>(envp), "PATH");
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

int run_execve(catter::ExecveFn* execve, const catter::Command& command, char* const envp[]) {
    if(execve == nullptr) {
        throw catter::PayloadError(ENOSYS, "hook function \"execve\" not initialized");
    }

    auto clean_env = catter::sanitize_environment(envp);
    auto c_argv = command.c_argv();
    INFO("execve called with path: {}, argv[0]: {}",
         command.path,
         c_argv[0] == nullptr ? "" : c_argv[0]);
    return execve(command.path.c_str(), c_argv.data(), clean_env.data());
}

int run_posix_spawn(catter::PosixSpawnFn* posix_spawn,
                    const catter::Command& command,
                    pid_t* pid,
                    const posix_spawn_file_actions_t* file_actions,
                    const posix_spawnattr_t* attrp,
                    char* const envp[]) {
    if(posix_spawn == nullptr) {
        throw catter::PayloadError(ENOSYS, "hook function \"posix_spawn\" not initialized");
    }

    auto clean_env = catter::sanitize_environment(envp);
    auto c_argv = command.c_argv();
    INFO("posix_spawn called with path: {}, argv[0]: {}",
         command.path,
         c_argv[0] == nullptr ? "" : c_argv[0]);
    return posix_spawn(pid,
                       command.path.c_str(),
                       file_actions,
                       attrp,
                       c_argv.data(),
                       clean_env.data());
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

void Executor::init(const char** envp) noexcept {
    m_session = Session::make(envp);

    try {
        m_execve = resolve_execve();
        m_posix_spawn = resolve_posix_spawn();
    } catch(const std::exception& err) {
        ERROR("failed to initialize hook target: {}", err.what());
        m_execve = nullptr;
    } catch(...) {
        ERROR("failed to initialize hook target with unknown exception");
        m_execve = nullptr;
    }
}

void Executor::init(Session session, ExecveFn* execve, PosixSpawnFn* posix_spawn) noexcept {
    m_session = session;
    m_execve = execve;
    m_posix_spawn = posix_spawn;
}

int Executor::execve(const char* path, char* const* argv, char* const* envp) noexcept {
    CATTER_EXEC_BOUNDARY("execve", {
        require_path_arg(path, "path");

        auto args = argv_span(argv);
        if(!m_session.is_valid()) {
            auto command = invalid_session_command(m_session, path, args);
            return run_execve(m_execve, command, envp);
        }

        auto executable = resolve_path_like(path);
        auto command = build_proxy_command(m_session, executable, args);
        return run_execve(m_execve, command, envp);
    });
}

int Executor::execv(const char* path, char* const* argv, char* const* envp) noexcept {
    CATTER_EXEC_BOUNDARY("execv", {
        require_path_arg(path, "path");

        auto args = argv_span(argv);
        if(!m_session.is_valid()) {
            auto command = invalid_session_command(m_session, path, args);
            return run_execve(m_execve, command, envp);
        }

        auto executable = resolve_path_like(path);
        auto command = build_proxy_command(m_session, executable, args);
        return run_execve(m_execve, command, envp);
    });
}

int Executor::execvpe(const char* file, char* const* argv, char* const* envp) noexcept {
    CATTER_EXEC_BOUNDARY("execvpe", {
        require_path_arg(file, "file");

        auto args = argv_span(argv);
        if(!m_session.is_valid()) {
            auto command = invalid_session_command(m_session, file, args);
            return run_execve(m_execve, command, envp);
        }

        auto executable = resolve_from_path(file, envp);
        auto command = build_proxy_command(m_session, executable, args);
        return run_execve(m_execve, command, envp);
    });
}

int Executor::execvp(const char* file, char* const* argv, char* const* envp) noexcept {
    CATTER_EXEC_BOUNDARY("execvp", {
        require_path_arg(file, "file");

        auto args = argv_span(argv);
        if(!m_session.is_valid()) {
            auto command = invalid_session_command(m_session, file, args);
            return run_execve(m_execve, command, envp);
        }

        auto executable = resolve_from_path(file, envp);
        auto command = build_proxy_command(m_session, executable, args);
        return run_execve(m_execve, command, envp);
    });
}

int Executor::execvP(const char* file,
                     const char* search_path,
                     char* const* argv,
                     char* const* envp) noexcept {
    CATTER_EXEC_BOUNDARY("execvP", {
        require_path_arg(file, "file");

        auto args = argv_span(argv);
        if(!m_session.is_valid()) {
            auto command = invalid_session_command(m_session, file, args);
            return run_execve(m_execve, command, envp);
        }

        require_path_arg(search_path, "search_path");
        auto executable = resolve_from_search_path(file, search_path);
        auto command = build_proxy_command(m_session, executable, args);
        return run_execve(m_execve, command, envp);
    });
}

int Executor::exect(const char* path, char* const* argv, char* const* envp) noexcept {
    CATTER_EXEC_BOUNDARY("exect", {
        require_path_arg(path, "path");

        auto args = argv_span(argv);
        if(!m_session.is_valid()) {
            auto command = invalid_session_command(m_session, path, args);
            return run_execve(m_execve, command, envp);
        }

        auto executable = resolve_path_like(path);
        auto command = build_proxy_command(m_session, executable, args);
        return run_execve(m_execve, command, envp);
    });
}

int Executor::execl(const char* path, char* const* argv, char* const* envp) noexcept {
    CATTER_EXEC_BOUNDARY("execl", {
        require_path_arg(path, "path");

        auto args = argv_span(argv);
        if(!m_session.is_valid()) {
            auto command = invalid_session_command(m_session, path, args);
            return run_execve(m_execve, command, envp);
        }

        auto executable = resolve_path_like(path);
        auto command = build_proxy_command(m_session, executable, args);
        return run_execve(m_execve, command, envp);
    });
}

int Executor::execlp(const char* file, char* const* argv, char* const* envp) noexcept {
    CATTER_EXEC_BOUNDARY("execlp", {
        require_path_arg(file, "file");

        auto args = argv_span(argv);
        if(!m_session.is_valid()) {
            auto command = invalid_session_command(m_session, file, args);
            return run_execve(m_execve, command, envp);
        }

        auto executable = resolve_from_path(file, envp);
        auto command = build_proxy_command(m_session, executable, args);
        return run_execve(m_execve, command, envp);
    });
}

int Executor::execle(const char* path, char* const* argv, char* const* envp) noexcept {
    CATTER_EXEC_BOUNDARY("execle", {
        require_path_arg(path, "path");

        auto args = argv_span(argv);
        if(!m_session.is_valid()) {
            auto command = invalid_session_command(m_session, path, args);
            return run_execve(m_execve, command, envp);
        }

        auto executable = resolve_path_like(path);
        auto command = build_proxy_command(m_session, executable, args);
        return run_execve(m_execve, command, envp);
    });
}

int Executor::posix_spawn(pid_t* pid,
                          const char* path,
                          const posix_spawn_file_actions_t* file_actions,
                          const posix_spawnattr_t* attrp,
                          char* const* argv,
                          char* const* envp) noexcept {
    CATTER_SPAWN_BOUNDARY("posix_spawn", {
        require_path_arg(path, "path");

        auto args = argv_span(argv);
        if(!m_session.is_valid()) {
            auto command = invalid_session_command(m_session, path, args);
            return run_posix_spawn(m_posix_spawn, command, pid, file_actions, attrp, envp);
        }

        auto executable = resolve_path_like(path);
        auto command = build_proxy_command(m_session, executable, args);
        return run_posix_spawn(m_posix_spawn, command, pid, file_actions, attrp, envp);
    });
}

int Executor::posix_spawnp(pid_t* pid,
                           const char* file,
                           const posix_spawn_file_actions_t* file_actions,
                           const posix_spawnattr_t* attrp,
                           char* const* argv,
                           char* const* envp) noexcept {
    CATTER_SPAWN_BOUNDARY("posix_spawnp", {
        require_path_arg(file, "file");

        auto args = argv_span(argv);
        if(!m_session.is_valid()) {
            auto command = invalid_session_command(m_session, file, args);
            return run_posix_spawn(m_posix_spawn, command, pid, file_actions, attrp, envp);
        }

        auto executable = resolve_from_path(file, envp);
        auto command = build_proxy_command(m_session, executable, args);
        return run_posix_spawn(m_posix_spawn, command, pid, file_actions, attrp, envp);
    });
}

}  // namespace catter

#undef CATTER_EXEC_BOUNDARY
#undef CATTER_SPAWN_BOUNDARY
