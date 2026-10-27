#pragma once

#include <cstdarg>
#include <string>
#include <vector>
#include <spawn.h>

namespace catter::hook {

using ExecveFn = int(const char* path, char* const argv[], char* const envp[]);
using PosixSpawnFn = int(pid_t* pid,
                         const char* path,
                         const posix_spawn_file_actions_t* file_actions,
                         const posix_spawnattr_t* attrp,
                         char* const argv[],
                         char* const envp[]);

/// Runs what the exec family and posix_spawn are asked to, finding the program
/// as each does (https://www.man7.org/linux/man-pages/man3/exec.3.html), and
/// starting catter-proxy in its place: it reports the command to the session,
/// and runs it as catter decides. What the system would fail, or run without
/// catter-proxy, goes to the system as it is.
class Executor {
public:
    /// Takes the session from `envp`, this process's, and starts programs with
    /// `execve` and `posix_spawn`, the C library's. The calls of a process
    /// whose environment names no catter-proxy go to the C library as they
    /// are.
    void init(const char* const envp[], ExecveFn* execve, PosixSpawnFn* posix_spawn) noexcept;

    int execv(const char* path, char* const argv[]) noexcept;

    int execve(const char* path, char* const argv[], char* const envp[]) noexcept;

    int execvp(const char* file, char* const argv[]) noexcept;

    /// glibc's.
    int execvpe(const char* file, char* const argv[], char* const envp[]) noexcept;

    int execl(const char* path, const char* arg, va_list* ap) noexcept;

    int execle(const char* path, const char* arg, va_list* ap) noexcept;

    int execlp(const char* file, const char* arg, va_list* ap) noexcept;

    /// macOS's, as the BSDs have it.
    int execvP(const char* file,
               const char* search_path,
               char* const argv[],
               char* const envp[]) noexcept;

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
    /// `shell_fallback`: the libc function this stands for runs a file of no
    /// executable format with /bin/sh, as execvp does.
    int run_execve(const char* executable,
                   const char* const argv[],
                   char* const envp[],
                   bool shell_fallback);

    int run_posix_spawn(pid_t* pid,
                        const char* executable,
                        const posix_spawn_file_actions_t* file_actions,
                        const posix_spawnattr_t* attrp,
                        const char* const argv[],
                        char* const envp[],
                        bool shell_fallback);

private:
    /// catter-proxy; empty in a process with no session.
    std::string proxy;

    /// The entries of the variables that name the session to catter-proxy.
    std::vector<std::string> session;

    ExecveFn* original_execve = nullptr;

    PosixSpawnFn* original_posix_spawn = nullptr;
};

}  // namespace catter::hook
