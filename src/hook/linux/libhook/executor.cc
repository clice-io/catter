#include "executor.h"

#include "array.h"
#include "buffer.h"
#include "config.h"
#include "debug.h"
#include "environment.h"
#include "io.h"
#include "paths.h"
#include "resolver.h"
#include "linker.h"
#include "session.h"

#include <cerrno>
#include <cstdlib>
#include <expected>
#include <linux/limits.h>
#include <unistd.h>

namespace {
#define INIT_EXEC() bool able_to_inject = true;

#define CHECK_SESSION(SESSION_)                                                                    \
    do {                                                                                           \
        if(!catter::session::is_valid(SESSION_)) {                                                 \
            recorder_.writeErr(                                                                    \
                "invalid enviroment variables of hook library, lost required value");              \
            able_to_inject = false;                                                                \
        }                                                                                          \
    } while(false)

#define CHECK_POINTER(PTR_)                                                                        \
    do {                                                                                           \
        if(nullptr == (PTR_)) {                                                                    \
            errno = EFAULT;                                                                        \
            return -1;                                                                             \
        }                                                                                          \
    } while(false)
#define LOGGING_CMD(PATH_, ARGV_)                                                                  \
    INFO("LOGGING_CMD");                                                                           \
    if(able_to_inject) {                                                                           \
        char cmd_buf[PATH_MAX * 4];                                                                \
        char* it = cmd_buf;                                                                        \
        if(build_cmd(it, cmd_buf + sizeof(cmd_buf), const_cast<char*>(PATH_), ARGV_) == 0) {       \
            recorder_.writeCmd(cmd_buf);                                                           \
        } else {                                                                                   \
            recorder_.writeErr("failed to build command string");                                  \
        }                                                                                          \
    }

#define INJECT_PRELOAD(SESSION_, ENVP_)                                                            \
    do {                                                                                           \
        INFO("INJECT_PRELOAD");                                                                    \
        if(able_to_inject) {                                                                       \
            if(inject_preload(SESSION_, ENVP_) != 0) {                                             \
                recorder_.writeErr("failed to inject preload library path");                       \
                able_to_inject = false;                                                            \
            }                                                                                      \
        }                                                                                          \
    } while(false)

#define CHECK_EXEC_RESULT(RES_)                                                                    \
    do {                                                                                           \
        INFO("CHECK_EXEC_RESULT");                                                                 \
        if(!RES_.has_value()) {                                                                    \
            recorder_.writeErr("Unable to locate executable");                                     \
            return RES_.error();                                                                   \
        }                                                                                          \
    } while(false)

int inject_catter_env(const catter::Session& ss, char* const*& envp_) {
    constexpr static auto envp_size = PATH_MAX;
    const static char* envp_copy[envp_size];
    if(!catter::session::is_valid(ss)) {
        return -1;
    }
    const char** envp = const_cast<const char**>(envp_);

    // first replace envp if it do not have env in session
    catter::Session test_ses;
    catter::session::from(test_ses, envp);
    if(!catter::session::is_valid(test_ses)) {
        INFO("copy envp into new area");
        size_t idx = 0;
        envp_copy[idx++] = ss.necessary_envp_entry[0];
        envp_copy[idx++] = ss.necessary_envp_entry[1];
        for(const char* it = envp; it != nullptr; ++it) {
            envp_copy[idx++] = it;
            if(idx >= (envp_size - 1)) {
                // no enough space
                WARN("no enough space to copy envp!");
                return -1;
            }
        }
        // check it has LD_PRELOAD
        if(catter::env::get_env_value(envp_copy, catter::config::KEY_PRELOAD) == nullptr) {
            // add a null entry for LD_PRELOAD
            envp_copy[idx++] = catter::config::LD_PRELOAD_INIT_ENTRY;
        }
        envp_copy[idx] = nullptr;
        envp_ = const_cast<char**>(envp_copy);
    }

    return 0;
};

/// add our catter hook library to LD_PRELOAD
int inject_preload(const catter::Session& ss, char* const*& envp_) {
    constexpr static auto area_size =
        PATH_MAX * 10 + 20 + catter::array::length(catter::config::KEY_PRELOAD);
    static char area_for_path[area_size];

    if(!catter::session::is_valid(ss)) {
        return -1;
    }

    // first replace envp if it do not have env in session
    if(auto res = inject_catter_env(ss, envp_); res != 0) {
        WARN("failed to inject catter env");
        return res;
    }
    const char** envp = const_cast<const char**>(envp_);

    const char* self_lib_path = ss.self_lib_path;
    if(self_lib_path == nullptr) {
        return -1;
    }
    const auto self_lib_path_sz = catter::array::length(self_lib_path);
    const char* new_ld_preload = catter::env::get_env_value(envp, catter::config::KEY_PRELOAD);
    if(new_ld_preload == nullptr) {
        new_ld_preload = "";
    }
    const auto new_ld_preload_sz = catter::array::length(new_ld_preload);
    for(const auto& path: catter::Paths(new_ld_preload)) {
        if(catter::array::equal_n(path.data(), self_lib_path, self_lib_path_sz)) {
            if(path.data()[self_lib_path_sz] == '\0') {
                // we already have it
                return 0;
            }
        }
    }
    // insert it in the front, reserved for ':' '\0'
    if(new_ld_preload_sz + self_lib_path_sz + catter::array::length(catter::config::KEY_PRELOAD) +
           5 >=
       area_size) {
        // we have no space to insert it.
        WARN("no enough space to inject preload path!");
        return -1;
    }

    // KEY=our_lib:old_value\0
    catter::Buffer buf(area_for_path, area_for_path + area_size);
    if(buf.store(catter::config::KEY_PRELOAD) == nullptr) {
        return -1;
    }
    if(buf.store("=") == nullptr) {
        return -1;
    }
    if(buf.store(self_lib_path) == nullptr) {
        return -1;
    }
    if(new_ld_preload[0] != catter::config::OS_PATH_SEPARATOR) {
        if(buf.store(":") == nullptr) {
            return -1;
        }
    }
    // insert old value
    if(buf.store(new_ld_preload) == nullptr) {
        return -1;
    }
    if(buf.store("\0") == nullptr) {
        return -1;
    }
    // replace the environment value
    int ret = catter::env::replace_env_value(const_cast<char**>(envp),
                                             catter::config::KEY_PRELOAD,
                                             area_for_path);
    return ret;
};

int build_cmd(char*& it, char* end, char* const path, char* const argv[]) {
    // copy path
    INFO("build_cmd for {}", path);
    it = catter::array::copy(path, path + catter::array::length(path), it, end);
    if(it == nullptr) {
        return -1;
    }
    // copy args
    for(size_t idx = 1; argv[idx] != nullptr; ++idx) {
        *it = ' ';
        it++;
        it = catter::array::copy(argv[idx], argv[idx] + catter::array::length(argv[idx]), it, end);
        if(it == nullptr) {
            return -1;
        }
    }
    *it = '\0';
    return 0;
}
}  // namespace

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvla"

namespace catter {

/// We separate the log and execute process, ensuring that even if the logging fails,
/// the execution can still proceed.
Executor::Executor(const Linker& linker,
                   const Session& session,
                   Resolver& resolver,
                   Recorder& recorder) noexcept :
    linker_(linker), session_(session), resolver_(resolver), recorder_(recorder) {}

int Executor::execve(const char* path, char* const* argv, char* const* envp) {
    INIT_EXEC();
    CHECK_SESSION(session_);
    CHECK_POINTER(path);

    auto executable_res = resolver_.from_current_directory(path);
    CHECK_EXEC_RESULT(executable_res);
    INJECT_PRELOAD(session_, envp);
    LOGGING_CMD(executable_res.value(), argv);
    // check envp
    auto run_res = linker_.execve(executable_res.value(), argv, envp);
    if(!run_res.has_value()) {
        recorder_.writeErr(run_res.error());
        errno = ENOSYS;
        return -1;
    }
    return run_res.value();
}

int Executor::execvpe(const char* file, char* const* argv, char* const* envp) {
    INIT_EXEC();
    CHECK_SESSION(session_);
    CHECK_POINTER(file);
    auto executable_res = resolver_.from_path(file, const_cast<const char**>(envp));
    CHECK_EXEC_RESULT(executable_res);
    INJECT_PRELOAD(session_, envp);
    LOGGING_CMD(executable_res.value(), argv);
    auto run_res = linker_.execve(executable_res.value(), argv, envp);
    if(!run_res.has_value()) {
        recorder_.writeErr(run_res.error());
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
    CHECK_SESSION(session_);
    CHECK_POINTER(file);

    auto executable_res = resolver_.from_search_path(file, search_path);
    CHECK_EXEC_RESULT(executable_res);
    INJECT_PRELOAD(session_, envp);
    LOGGING_CMD(executable_res.value(), argv);
    auto run_res = linker_.execve(executable_res.value(), argv, envp);
    if(!run_res.has_value()) {
        recorder_.writeErr(run_res.error());
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
    CHECK_SESSION(session_);
    CHECK_POINTER(path);

    auto executable_res = resolver_.from_current_directory(path);
    CHECK_EXEC_RESULT(executable_res);
    INJECT_PRELOAD(session_, envp);
    LOGGING_CMD(executable_res.value(), argv);
    auto run_res =
        linker_.posix_spawn(pid, executable_res.value(), file_actions, attrp, argv, envp);
    if(!run_res.has_value()) {
        recorder_.writeErr(run_res.error());
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
    CHECK_SESSION(session_);
    CHECK_POINTER(file);

    auto executable_res = resolver_.from_path(file, const_cast<const char**>(envp));
    CHECK_EXEC_RESULT(executable_res);
    INJECT_PRELOAD(session_, envp);
    LOGGING_CMD(executable_res.value(), argv);
    auto run_res =
        linker_.posix_spawn(pid, executable_res.value(), file_actions, attrp, argv, envp);
    if(!run_res.has_value()) {
        recorder_.writeErr(run_res.error());
        errno = ENOSYS;
        return -1;
    }
    return run_res.value();
}
}  // namespace catter

#pragma GCC diagnostic pop
