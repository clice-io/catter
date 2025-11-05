#include "executor.h"

#include "array.h"
#include "config.h"
#include "environment.h"
#include "paths.h"
#include "resolver.h"
#include "linker.h"
#include "session.h"

#include <cerrno>
#include <expected>
#include <unistd.h>

namespace {

#define CHECK_SESSION(SESSION_)                                                                    \
    do {                                                                                           \
        if(!catter::session::is_valid(SESSION_)) {                                                 \
            return std::unexpected(EIO);                                                           \
        }                                                                                          \
    } while(false)

#define CHECK_POINTER(PTR_)                                                                        \
    do {                                                                                           \
        if(nullptr == (PTR_)) {                                                                    \
            return std::unexpected(EFAULT);                                                        \
        }                                                                                          \
    } while(false)

/// add our catter hook library to LD_PRELOAD
int inject_preload(const catter::Session& ss, char* const* envp_) {
    const char** envp = const_cast<const char**>(envp_);
    constexpr static auto area_size =
        PATH_MAX * 10 + 20 + catter::array::length(catter::config::KEY_PRELOAD);
    static char area_for_path[area_size];

    const char* self_lib_path = ss.self_lib_path;
    if(self_lib_path == nullptr) {
        return -1;
    }
    const auto self_lib_path_sz = catter::array::length(self_lib_path);

    const char* new_ld_preload = catter::env::get_env_value(envp, catter::config::KEY_PRELOAD);
    const auto new_ld_preload_sz = catter::array::length(new_ld_preload);

    for(const auto& path: catter::SearchPaths(new_ld_preload)) {
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
        return -1;
    }
    auto it = area_for_path;
    // insert key =
    it = catter::array::copy(catter::config::KEY_PRELOAD,
                             catter::array::end(catter::config::KEY_PRELOAD),
                             it,
                             area_for_path + area_size);
    *it = '=';
    it++;
    // insert our library path, ':'
    it = catter::array::copy(self_lib_path,
                             catter::array::end(self_lib_path),
                             it,
                             area_for_path + area_size);
    if(new_ld_preload[0] != catter::config::OS_PATH_SEPARATOR) {
        *it = catter::config::OS_PATH_SEPARATOR;
        it++;
    }
    // insert old value
    it = catter::array::copy(new_ld_preload,
                             catter::array::end(new_ld_preload),
                             it,
                             area_for_path + area_size);
    *it = '\0';
    // replace the environment value
    int ret = catter::env::replace_env_value(const_cast<char**>(envp),
                                             catter::config::KEY_PRELOAD,
                                             area_for_path);
    return (it == nullptr) ? -1 : 0;
};

}  // namespace

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvla"

namespace catter {

Executor::Executor(const Linker& linker, const Session& session, Resolver& resolver) noexcept :
    linker_(linker), session_(session), resolver_(resolver) {}

std::expected<int, int> Executor::execve(const char* path,
                                         char* const* argv,
                                         char* const* envp) const {
    CHECK_SESSION(session_);
    CHECK_POINTER(path);

    if(auto executable = resolver_.from_current_directory(path); executable.has_value()) {
        if(inject_preload(session_, envp) != 0) {
            // TODO: inject error
        }
        return linker_.execve(executable.value(), argv, envp);
    } else {
        return std::unexpected(executable.error());
    }
}

std::expected<int, int> Executor::execvpe(const char* file,
                                          char* const* argv,
                                          char* const* envp) const {
    CHECK_SESSION(session_);
    CHECK_POINTER(file);

    if(auto executable = resolver_.from_path(file, const_cast<const char**>(envp));
       executable.has_value()) {
        if(inject_preload(session_, envp) != 0) {
            // TODO
        }
        return linker_.execve(executable.value(), argv, envp);
    } else {
        return std::unexpected(executable.error());
    }
}

std::expected<int, int> Executor::execvP(const char* file,
                                         const char* search_path,
                                         char* const* argv,
                                         char* const* envp) const {
    CHECK_SESSION(session_);
    CHECK_POINTER(file);

    if(auto executable = resolver_.from_search_path(file, search_path); executable.has_value()) {
        if(inject_preload(session_, envp) != 0) {
            // TODO
        }
        return linker_.execve(executable.value(), argv, envp);
    } else {
        return std::unexpected(executable.error());
    }
}

std::expected<int, int> Executor::posix_spawn(pid_t* pid,
                                              const char* path,
                                              const posix_spawn_file_actions_t* file_actions,
                                              const posix_spawnattr_t* attrp,
                                              char* const* argv,
                                              char* const* envp) const {
    CHECK_SESSION(session_);
    CHECK_POINTER(path);

    if(auto executable = resolver_.from_current_directory(path); executable.value()) {
        if(inject_preload(session_, envp) != 0) {
            // TODO
        }
        return linker_.posix_spawn(pid, executable.value(), file_actions, attrp, argv, envp);
    } else {
        return std::unexpected(executable.error());
    }
}

std::expected<int, int> Executor::posix_spawnp(pid_t* pid,
                                               const char* file,
                                               const posix_spawn_file_actions_t* file_actions,
                                               const posix_spawnattr_t* attrp,
                                               char* const* argv,
                                               char* const* envp) const {
    CHECK_SESSION(session_);
    CHECK_POINTER(file);

    if(auto executable = resolver_.from_path(file, const_cast<const char**>(envp));
       executable.has_value()) {
        if(inject_preload(session_, envp) != 0) {
            // TODO
        }
        return linker_.posix_spawn(pid, executable.value(), file_actions, attrp, argv, envp);
    } else {
        return std::unexpected(executable.error());
    }
}
}  // namespace catter

#pragma GCC diagnostic pop
