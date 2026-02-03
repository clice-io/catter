#include "executor.h"

#include "command.h"
#include "linux-mac/debug.h"
#include "resolver.h"
#include "linker.h"
#include "session.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <expected>
#include <limits.h>
#include <span>
#include <unistd.h>

namespace {

#define ELSE_RETURN(NAME, OPTION)                                                                  \
    auto NAME##_res = OPTION;                                                                      \
    if(!NAME##_res.has_value()) {                                                                  \
        errno = NAME##_res.error();                                                                \
        return -1;                                                                                 \
    }                                                                                              \
    auto NAME = NAME##_res.value();

std::expected<catter::CmdBuilder::command, int>
    precheck(const catter::Session& sess,
             const char* str,
             catter::CmdBuilder::ArgvRef argv) noexcept {
    if(str == nullptr) {
        errno = EFAULT;
        return std::unexpected(-1);
    }
    if(!catter::session::is_valid(sess)) {
        return catter::CmdBuilder(sess).error_cmd(
            "invalid environment of hook library, lost required value",
            str,
            argv);
    }
    return {};
}

catter::CmdBuilder::ArgvRef spanify(char* const* argv) {
    int argc = 0;
    if(argv == nullptr) {
        return {};
    }
    while(argv[argc] != 0) {
        argc++;
    }
    return std::span(argv, argc);
}

}  // namespace

namespace catter {

/// We separate the log and execute process, ensuring that even if the logging fails,
/// the execution can still proceed.
Executor::Executor(const Linker& linker,
                   const Session& session,
                   const Resolver& resolver /* = {}*/) noexcept :
    linker_(linker), session_(session), resolver_(resolver), cmd_builder_(session) {}

int Executor::execve(const char* path, char* const* argv, char* const* envp) {
    auto argv_ref = spanify(argv);
    ELSE_RETURN(cmd, precheck(session_, path, argv_ref))
    ELSE_RETURN(executable, resolver_.from_current_directory(path));
    // if no error, we build it
    if(!cmd.valid()) {
        cmd = cmd_builder_.proxy_cmd(executable, argv_ref);
    }
    auto run_res =
        linker_.execve(cmd.path.c_str(), const_cast<decltype(argv)>(cmd.c_argv().data()), envp);
    if(!run_res.has_value()) {
        ERROR("execve failed: {}", run_res.error());
        errno = ENOSYS;
        return -1;
    }
    return run_res.value();
}

int Executor::execvpe(const char* file, char* const* argv, char* const* envp) {
    auto argv_ref = spanify(argv);
    ELSE_RETURN(cmd, precheck(session_, file, argv_ref));
    ELSE_RETURN(executable, resolver_.from_path(file, const_cast<const char**>(envp)));
    // if no error, we build it
    if(!cmd.valid()) {
        cmd = cmd_builder_.proxy_cmd(executable, argv_ref);
    }
    auto run_res =
        linker_.execve(cmd.path.c_str(), const_cast<decltype(argv)>(cmd.c_argv().data()), envp);
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
    auto argv_ref = spanify(argv);
    ELSE_RETURN(cmd, precheck(session_, file, argv_ref));
    ELSE_RETURN(executable, resolver_.from_search_path(file, search_path));
    // if no error, we build it
    if(!cmd.valid()) {
        cmd = cmd_builder_.proxy_cmd(executable, argv_ref);
    }
    auto run_res =
        linker_.execve(cmd.path.c_str(), const_cast<decltype(argv)>(cmd.c_argv().data()), envp);
    if(!run_res.has_value()) {
        ERROR("execvP failed: {}", run_res.error());
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
    auto argv_ref = spanify(argv);
    ELSE_RETURN(cmd, precheck(session_, path, argv_ref));
    ELSE_RETURN(executable, resolver_.from_current_directory(path));
    // if no error, we build it
    if(!cmd.valid()) {
        cmd = cmd_builder_.proxy_cmd(executable, argv_ref);
    }
    auto run_res = linker_.posix_spawn(pid,
                                       cmd.path.c_str(),
                                       file_actions,
                                       attrp,
                                       const_cast<decltype(argv)>(cmd.c_argv().data()),
                                       envp);
    if(!run_res.has_value()) {
        errno = ENOSYS;
        ERROR("posix_spawn failed: {}", run_res.error());
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
    auto argv_ref = spanify(argv);
    ELSE_RETURN(cmd, precheck(session_, file, argv_ref));
    ELSE_RETURN(executable, resolver_.from_path(file, const_cast<const char**>(envp)));
    // if no error, we build it
    if(!cmd.valid()) {
        cmd = cmd_builder_.proxy_cmd(executable, argv_ref);
    }
    auto run_res = linker_.posix_spawn(pid,
                                       cmd.path.c_str(),
                                       file_actions,
                                       attrp,
                                       const_cast<decltype(argv)>(cmd.c_argv().data()),
                                       envp);
    if(!run_res.has_value()) {
        errno = ENOSYS;
        ERROR("posix_spawnp failed: {}", run_res.error());
        return -1;
    }
    return run_res.value();
}
}  // namespace catter
