#pragma once

#include <spawn.h>

#include "session.h"

namespace catter {

using ExecveFn = int(const char* path, char* const argv[], char* const envp[]);
using PosixSpawnFn = int(pid_t* pid,
                         const char* path,
                         const posix_spawn_file_actions_t* file_actions,
                         const posix_spawnattr_t* attrp,
                         char* const argv[],
                         char* const envp[]);

class Executor {
public:
    void init(const char* const envp[]) noexcept;
    void init(Session session, ExecveFn* execve, PosixSpawnFn* posix_spawn) noexcept;

    int execv(const char* path, char* const argv[]) noexcept;

    int execve(const char* path, char* const argv[], char* const envp[]) noexcept;

    int execvp(const char* file, char* const argv[]) noexcept;

    // GNU extension
    int execvpe(const char* file, char* const argv[], char* const envp[]) noexcept;

    int execl(const char* path, const char* const argv[]) noexcept;

    int execle(const char* path, const char* const argv[], char* const envp[]) noexcept;

    int execlp(const char* file, const char* const argv[]) noexcept;

    // FreeBSD specific extension
    int execvP(const char* file,
               const char* search_path,
               char* const argv[],
               char* const envp[]) noexcept;

    // BSD/Latency Unix specific extension
    int exect(const char* path, char* const argv[], char* const envp[]) noexcept;

    int posix_spawn(pid_t* pid,
                    const char* path,
                    const posix_spawn_file_actions_t* file_actions,
                    const posix_spawnattr_t* attrp,
                    char* const argv[],
                    char* const envp[]) noexcept;

    int posix_spawnp(pid_t* pid,
                     const char* file,
                     const posix_spawn_file_actions_t* file_actions,
                     const posix_spawnattr_t* attrp,
                     char* const argv[],
                     char* const envp[]) noexcept;

private:
    int run_execve(const char* executable, const char* const argv[], char* const envp[]);

    int run_posix_spawn(pid_t* pid,
                        const char* executable,
                        const posix_spawn_file_actions_t* file_actions,
                        const posix_spawnattr_t* attrp,
                        const char* const argv[],
                        char* const envp[]);
    Session m_session;
    ExecveFn* m_execve = nullptr;
    PosixSpawnFn* m_posix_spawn = nullptr;
};

}  // namespace catter
