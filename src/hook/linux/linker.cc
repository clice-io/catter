#include "./linker.h"
#include <cerrno>
#include <dlfcn.h>
#include <expected>
#include <spawn.h>

using execve_t = int (*)(const char* path, char* const argv[], char* const envp[]);
using posix_spawn_t = int (*)(pid_t* pid,
                              const char* path,
                              const posix_spawn_file_actions_t* file_actions,
                              const posix_spawnattr_t* attrp,
                              char* const argv[],
                              char* const envp[]);

namespace {
template <typename T>
T dynamic_linker(const char* const name) {
    return reinterpret_cast<T>(dlsym(RTLD_NEXT, name));
}
}  // namespace

namespace catter {

std::expected<int, int> Linker::execve(const char* path,
                                       char* const* argv,
                                       char* const* envp) const noexcept {
    const auto fp = dynamic_linker<execve_t>("execve");
    if(fp == nullptr) {
        return std::unexpected(EINVAL);
    }
    auto result = fp(path, argv, envp);
    if(result == -1) {
        return std::unexpected(errno);
    }
    return result;
};

std::expected<int, int> Linker::posix_spawn(pid_t* pid,
                                            const char* path,
                                            const posix_spawn_file_actions_t* file_actions,
                                            const posix_spawnattr_t* attrp,
                                            char* const* argv,
                                            char* const* envp) const noexcept {

    const auto fp = dynamic_linker<posix_spawn_t>("posix_spawn");
    if(fp == nullptr) {
        return std::unexpected(EINVAL);
    }
    auto result = fp(pid, path, file_actions, attrp, argv, envp);
    if(result == -1) {
        return std::unexpected(errno);
    }
    return result;
};
}  // namespace catter
