#include "executor.h"

#include "command.h"
#include "libhook/linux-mac/debug.h"
#include "resolver.h"
#include "linker.h"
#include "session.h"

#include <cerrno>
#include <cstdlib>
#include <expected>
#include <limits.h>
#include <unistd.h>

namespace {

#define INIT_EXEC() CmdBuilder::command final_cmd_or_error = {nullptr, nullptr};

#define CHECK_SESSION(SESSION_, EXEC, ARGV)                                                        \
    do {                                                                                           \
        if(!catter::session::is_valid(SESSION_)) {                                                 \
            final_cmd_or_error = cmd_builder_.error_str(                                           \
                "invalid enviroment variables of hook library, lost required value",               \
                EXEC,                                                                              \
                ARGV);                                                                             \
        }                                                                                          \
    } while(false)

#define CHECK_POINTER(PTR_)                                                                        \
    do {                                                                                           \
        if(nullptr == (PTR_)) {                                                                    \
            errno = EFAULT;                                                                        \
            return -1;                                                                             \
        }                                                                                          \
    } while(false)

#define CHECK_EXEC_RESULT(RES_, EXEC, ARGV)                                                        \
    do {                                                                                           \
        INFO("CHECK_EXEC_RESULT");                                                                 \
        if(!RES_.has_value()) {                                                                    \
            final_cmd_or_error =                                                                   \
                cmd_builder_.error_str("Unable to locate executable", EXEC, ARGV);                 \
            return RES_.error();                                                                   \
        }                                                                                          \
    } while(false)

}  // namespace

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvla"

namespace catter {

/// We separate the log and execute process, ensuring that even if the logging fails,
/// the execution can still proceed.
Executor::Executor(const Linker& linker, const Session& session, Resolver& resolver) noexcept :
    linker_(linker), session_(session), resolver_(resolver),
    cmd_builder_(session.proxy_path, session_.self_id) {}

int Executor::execve(const char* path, char* const* argv, char* const* envp) {

    INIT_EXEC();
    CHECK_POINTER(path);
    CHECK_SESSION(session_, path, argv);

    auto executable_res = resolver_.from_current_directory(path);
    CHECK_EXEC_RESULT(executable_res, path, argv);
    // if no error, we build it
    if(!final_cmd_or_error.valid()) {
        final_cmd_or_error = cmd_builder_.proxy_str(executable_res.value(), argv);
    }
    auto [new_path, new_argv] = final_cmd_or_error;
    auto run_res = linker_.execve(new_path, new_argv, envp);
    if(!run_res.has_value()) {
        ERROR("execve failed: {}", run_res.error());
        errno = ENOSYS;
        return -1;
    }
    return run_res.value();
}

int Executor::execvpe(const char* file, char* const* argv, char* const* envp) {
    INIT_EXEC();
    CHECK_POINTER(file);
    CHECK_SESSION(session_, file, argv);
    auto executable_res = resolver_.from_path(file, const_cast<const char**>(envp));
    CHECK_EXEC_RESULT(executable_res, file, argv);
    if(!final_cmd_or_error.valid()) {
        final_cmd_or_error = cmd_builder_.proxy_str(executable_res.value(), argv);
    }
    auto [new_path, new_argv] = final_cmd_or_error;
    auto run_res = linker_.execve(new_path, new_argv, envp);
    if(!run_res.has_value()) {
        ERROR("execvpe failed: {}", run_res.error());
        errno = ENOSYS;
        return -1;
    }
    return run_res.value();
}

int Executor::execvP(const char* file,
                     const char* search_path,
                     char* const* argv,
                     char* const* envp) {
    INIT_EXEC();
    CHECK_POINTER(file);
    CHECK_SESSION(session_, file, argv);

    auto executable_res = resolver_.from_search_path(file, search_path);
    CHECK_EXEC_RESULT(executable_res, file, argv);
    if(!final_cmd_or_error.valid()) {
        final_cmd_or_error = cmd_builder_.proxy_str(executable_res.value(), argv);
    }
    auto [new_path, new_argv] = final_cmd_or_error;
    auto run_res = linker_.execve(new_path, new_argv, envp);
    if(!run_res.has_value()) {
        errno = ENOSYS;
        return -1;
    }
    return run_res.value();
}

int Executor::posix_spawn(pid_t* pid,
                          const char* path,
                          const posix_spawn_file_actions_t* file_actions,
                          const posix_spawnattr_t* attrp,
                          char* const* argv,
                          char* const* envp) {
    INIT_EXEC();
    CHECK_POINTER(path);
    CHECK_SESSION(session_, path, argv);

    auto executable_res = resolver_.from_current_directory(path);
    CHECK_EXEC_RESULT(executable_res, path, argv);
    if(!final_cmd_or_error.valid()) {
        final_cmd_or_error = cmd_builder_.proxy_str(executable_res.value(), argv);
    }
    auto [new_path, new_argv] = final_cmd_or_error;
    auto run_res = linker_.posix_spawn(pid, new_path, file_actions, attrp, new_argv, envp);
    if(!run_res.has_value()) {
        errno = ENOSYS;
        return -1;
    }
    return run_res.value();
}

int Executor::posix_spawnp(pid_t* pid,
                           const char* file,
                           const posix_spawn_file_actions_t* file_actions,
                           const posix_spawnattr_t* attrp,
                           char* const* argv,
                           char* const* envp) {
    INIT_EXEC();
    CHECK_POINTER(file);
    CHECK_SESSION(session_, file, argv);

    auto executable_res = resolver_.from_path(file, const_cast<const char**>(envp));
    CHECK_EXEC_RESULT(executable_res, file, argv);
    if(!final_cmd_or_error.valid()) {
        final_cmd_or_error = cmd_builder_.proxy_str(executable_res.value(), argv);
    }
    auto [new_path, new_argv] = final_cmd_or_error;
    auto run_res = linker_.posix_spawn(pid, new_path, file_actions, attrp, new_argv, envp);
    if(!run_res.has_value()) {
        errno = ENOSYS;
        return -1;
    }
    return run_res.value();
}
}  // namespace catter

#pragma GCC diagnostic pop
