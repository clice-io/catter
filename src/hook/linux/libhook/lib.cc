#define _GNU_SOURCE

#include "linker.h"
#include "session.h"
#include "resolver.h"
#include "executor.h"

#include <linux/limits.h>
#include "unistd.h"
#include <atomic>
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <dlfcn.h>
#include <spawn.h>

namespace {

/// helper for hook c function with var args

size_t va_length(va_list& args) {
    size_t arg_count = 0;
    while(va_arg(args, const char*) != nullptr)
        ++arg_count;
    return arg_count;
}

void va_copy_n(va_list& args, char* argv[], const size_t argc) {
    for(size_t idx = 0; idx < argc; ++idx)
        argv[idx] = va_arg(args, char*);
}

const char** environment() noexcept {
#ifdef HAVE_NSGETENVIRON
    return const_cast<const char**>(*_NSGetEnviron());
#else
    return const_cast<const char**>(environ);
#endif
}
}  // namespace

// dynamic linker

/// function type we need hook
using execve_t = int (*)(const char* path, char* const argv[], char* const envp[]);
using execv_t = int (*)(const char* path, char* const argv[]);
using execvpe_t = int (*)(const char* file, char* const argv[], char* const envp[]);
using execvp_t = int (*)(const char* file, char* const argv[]);

using execl_t = int (*)(const char* path, const char* arg, ...);
using execlp_t = int (*)(const char* file, const char* arg, ...);
/// int execle(const char *path, const char *arg, ..., char * const envp[]);
using execle_t = int (*)(const char* path, const char* arg, ...);

using posix_spawn_t = int (*)(pid_t* pid,
                              const char* path,
                              const posix_spawn_file_actions_t* file_actions,
                              const posix_spawnattr_t* attrp,
                              char* const argv[],
                              char* const envp[]);
using posix_spawnp_t = int (*)(pid_t* pid,
                               const char* file,
                               const posix_spawn_file_actions_t* file_actions,
                               const posix_spawnattr_t* attrp,
                               char* const argv[],
                               char* const envp[]);

/**
 * Library static data
 *
 * Will be initialized, when the library loaded into memory.
 */
namespace {

// This is the only non stack memory that this library is using.
constexpr size_t BUFFER_SIZE = PATH_MAX * 2;
char BUFFER[BUFFER_SIZE];
// This is used for being multi thread safe (loading time only).
std::atomic<bool> LOADED(false);
// These are related to the functionality of this library.
catter::Linker LINKER;
catter::Session SESSION;

}  // namespace

/**
 * Library entry point.
 *
 * The first method to call after the library is loaded into memory.
 */

namespace ct = catter;

extern "C" void on_load() __attribute__((constructor));

extern "C" void on_load() {
    // Test whether on_load was called already.
    if(LOADED.exchange(true))
        return;

    // TODO: initialization code here
    ct::session::from(SESSION, environment());
    catter::session::persist(SESSION, BUFFER, BUFFER + BUFFER_SIZE);

    errno = 0;
}

/**
 * Library exit point.
 *
 * The last method which needs to be called when the library is unloaded.
 */
extern "C" void on_unload() __attribute__((destructor));

extern "C" void on_unload() {
    // Test whether on_unload was called already.
    if(not LOADED.exchange(false))
        return;

    // TODO: cleanup code here

    errno = 0;
}

// TODO: implement hooked functions below

extern "C" int execve(const char* path, char* const argv[], char* const envp[]) {
    ct::Resolver resolver;
    const auto result = ct::Executor(LINKER, SESSION, resolver).execve(path, argv, envp);
    if(!result.has_value()) {
        errno = result.error();
    }
    return result.value_or(-1);
}

extern "C" int execv(const char* path, char* const argv[]) {}

extern "C" int execvpe(const char* file, char* const argv[], char* const envp[]) {}

extern "C" int execvp(const char* file, char* const argv[]) {}

extern "C" int execvP(const char* file, const char* search_path, char* const argv[]) {}

extern "C" int exect(const char* path, char* const argv[], char* const envp[]) {}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvla"

extern "C" int execl(const char* path, const char* arg, ...) {
    printf("execl path: %s", path);

    // Count the number of arguments.
    va_list ap;
    va_start(ap, arg);
    const size_t argc = va_length(ap);
    va_end(ap);
    // Copy the arguments to the stack.
    va_start(ap, arg);
    char* argv[argc + 2];
    argv[0] = const_cast<char*>(path);
    va_copy_n(ap, &argv[1], argc + 1);
    va_end(ap);

    auto envp = const_cast<char* const*>(environment());
}

extern "C" int execlp(const char* file, const char* arg, ...) {

    // Count the number of arguments.
    va_list ap;
    va_start(ap, arg);
    const size_t argc = va_length(ap);
    va_end(ap);
    // Copy the arguments to the stack.
    va_start(ap, arg);
    char* argv[argc + 2];
    argv[0] = const_cast<char*>(file);
    va_copy_n(ap, &argv[1], argc + 1);
    va_end(ap);

    auto envp = const_cast<char* const*>(environment());
}

// int execle(const char *path, const char *arg, ..., char * const envp[]);
extern "C" int execle(const char* path, const char* arg, ...) {

    // Count the number of arguments.
    va_list ap;
    va_start(ap, arg);
    const size_t argc = va_length(ap);
    va_end(ap);
    // Copy the arguments to the stack.
    va_start(ap, arg);
    char* argv[argc + 2];
    argv[0] = const_cast<char*>(path);
    va_copy_n(ap, &argv[1], argc + 1);
    char** envp = va_arg(ap, char**);
    va_end(ap);
}

#pragma GCC diagnostic pop

extern "C" int posix_spawn(pid_t* pid,
                           const char* path,
                           const posix_spawn_file_actions_t* file_actions,
                           const posix_spawnattr_t* attrp,
                           char* const argv[],
                           char* const envp[]) {}

extern "C" int posix_spawnp(pid_t* pid,
                            const char* file,
                            const posix_spawn_file_actions_t* file_actions,
                            const posix_spawnattr_t* attrp,
                            char* const argv[],
                            char* const envp[]) {}
