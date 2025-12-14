#include "./linker.h"
#include <cerrno>
#include <cstdlib>
#include <dlfcn.h>
#include <expected>
#include <spawn.h>
#include <unistd.h>
#include "linux-mac/debug.h"
#include "linux-mac/crossplat.h"

using execve_t = int (*)(const char* path, char* const argv[], char* const envp[]);
using posix_spawn_t = int (*)(pid_t* pid,
                              const char* path,
                              const posix_spawn_file_actions_t* file_actions,
                              const posix_spawnattr_t* attrp,
                              char* const argv[],
                              char* const envp[]);

namespace catter {

std::expected<int, const char*> Linker::execve(const char* path,
                                               char* const* argv,
                                               char* const* envp) const noexcept {
#ifdef CATTER_MAC
    const auto fp = &::execve;
#endif
#ifdef CATTER_LINUX
    const auto fp = dynamic_linker<execve_t>("execve");
#endif
    if(fp == nullptr) {
        return std::unexpected("hook function \"execve\" not found");
    }
    INFO("execve called with path: {}, argv[0]: {}", path, argv[0]);
    if(argv[1][1] != 'p') {
        WARN("execve called with unexpected argv[1]: {}", argv[1]);
    }
    auto result = fp(path, argv, envp);
    return result;
};

std::expected<int, const char*> Linker::posix_spawn(pid_t* pid,
                                                    const char* path,
                                                    const posix_spawn_file_actions_t* file_actions,
                                                    const posix_spawnattr_t* attrp,
                                                    char* const* argv,
                                                    char* const* envp) const noexcept {

#ifdef CATTER_MAC
    const auto fp = &::posix_spawn;
#endif
#ifdef CATTER_LINUX
    const auto fp = dynamic_linker<posix_spawn_t>("posix_spawn");
#endif
    if(fp == nullptr) {
        return std::unexpected("hook function \"posix_spawn\" not found");
    }
    INFO("execve called with path: {}, argv[0]: {}", path, argv[0]);
    if(argv[1][1] != 'p') {
        WARN("posix_spawn called with unexpected argv[1]: {}", argv[1]);
    }
    auto result = fp(pid, path, file_actions, attrp, argv, envp);
    return result;
};
}  // namespace catter
