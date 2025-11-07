#pragma once

#include <expected>
#include <spawn.h>

namespace catter {

/**
 * It is an abstraction of the symbol resolver.
 *
 * It uses the provided symbol resolver method and cast the result
 * to a specific type.
 */
struct Linker {
    virtual ~Linker() noexcept = default;

    [[nodiscard]]
    virtual std::expected<int, const char*> execve(const char* path,
                                                   char* const argv[],
                                                   char* const envp[]) const noexcept;

    [[nodiscard]]
    virtual std::expected<int, const char*>
        posix_spawn(pid_t* pid,
                    const char* path,
                    const posix_spawn_file_actions_t* file_actions,
                    const posix_spawnattr_t* attrp,
                    char* const argv[],
                    char* const envp[]) const noexcept;
};
}  // namespace catter
