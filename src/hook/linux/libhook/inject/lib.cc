#include <cstdlib>
#define _GNU_SOURCE

#include "io.h"
#include "linker.h"
#include "session.h"
#include "resolver.h"
#include "executor.h"
#include "debug.h"
#include "crossplat.h"

#include <limits.h>
#include "unistd.h"
#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <dlfcn.h>
#include <spawn.h>

#define EXPORT_SYMBOL __attribute__((visibility("default")))

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
catter::Recorder RECORDER;

}  // namespace

/**
 * Library entry point.
 *
 * The first method to call after the library is loaded into memory.
 */

namespace ct = catter;

extern "C" EXPORT_SYMBOL void on_load() __attribute__((constructor));

extern "C" EXPORT_SYMBOL void on_load() {
    // Test whether on_load was called already.
    if(LOADED.exchange(true))
        return;
    INFO("catter hook library loaded, from executable path: {}", get_executable_path());
    // TODO: initialization code here
    ct::session::from(SESSION, environment());
    catter::session::persist(SESSION, BUFFER, BUFFER + BUFFER_SIZE);
    ct::Recorder::build(RECORDER, SESSION.log_path);

    errno = 0;
}

/**
 * Library exit point.
 *
 * The last method which needs to be called when the library is unloaded.
 */
extern "C" EXPORT_SYMBOL void on_unload() __attribute__((destructor));

extern "C" EXPORT_SYMBOL void on_unload() {
    // Test whether on_unload was called already.
    if(not LOADED.exchange(false))
        return;
    INFO("catter hook library unloaded");
    // TODO: cleanup code here

    errno = 0;
}

// TODO: implement hooked functions below

extern "C" EXPORT_SYMBOL int HOOK_NAME(execve)(const char* path,
                                               char* const argv[],
                                               char* const envp[]) {
    ct::Resolver resolver;
    INFO("hooked execve called: path={}, argv[0]={}", path, argv[0]);
    return ct::Executor(LINKER, SESSION, resolver, RECORDER).execve(path, argv, envp);
}

INJECT_FUNCTION(execve);

extern "C" EXPORT_SYMBOL int HOOK_NAME(execv)(const char* path, char* const argv[]) {
    ct::Resolver resolver;
    auto envp = const_cast<char* const*>(environment());
    INFO("hooked execv called: path={}, argv[0]={}", path, argv[0]);
    return ct::Executor(LINKER, SESSION, resolver, RECORDER).execve(path, argv, envp);
}

INJECT_FUNCTION(execv);

extern "C" EXPORT_SYMBOL int HOOK_NAME(execvpe)(const char* file,
                                                char* const argv[],
                                                char* const envp[]) {
    ct::Resolver resolver;
    INFO("hooked execvpe called: file={}, argv[0]={}", file, argv[0]);
    return ct::Executor(LINKER, SESSION, resolver, RECORDER).execvpe(file, argv, envp);
}

// INJECT_FUNCTION(execvpe);

extern "C" EXPORT_SYMBOL int HOOK_NAME(execvp)(const char* file, char* const argv[]) {
    ct::Resolver resolver;
    auto envp = const_cast<char* const*>(environment());
    INFO("hooked execvp called: file={}, argv[0]={}", file, argv[0]);
    return ct::Executor(LINKER, SESSION, resolver, RECORDER).execvpe(file, argv, envp);
}

INJECT_FUNCTION(execvp);

extern "C" EXPORT_SYMBOL int HOOK_NAME(execvP)(const char* file,
                                               const char* search_path,
                                               char* const argv[]) {
    ct::Resolver resolver;
    auto envp = const_cast<char* const*>(environment());
    INFO("hooked execvP called: file={}, argv[0]={}", file, argv[0]);
    return ct::Executor(LINKER, SESSION, resolver, RECORDER).execvP(file, search_path, argv, envp);
}

INJECT_FUNCTION(execvP);

extern "C" EXPORT_SYMBOL int HOOK_NAME(exect)(const char* path,
                                              char* const argv[],
                                              char* const envp[]) {
    ct::Resolver resolver;
    INFO("hooked exect called: path={}, argv[0]={}", path, argv[0]);
    return ct::Executor(LINKER, SESSION, resolver, RECORDER).execve(path, argv, envp);
}

// INJECT_FUNCTION(exect);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvla"

extern "C" EXPORT_SYMBOL int HOOK_NAME(execl)(const char* path, const char* arg, ...) {

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
    ct::Resolver resolver;
    INFO("hooked execl called: path={}, argv[0]={}", path, argv[0]);
    return ct::Executor(LINKER, SESSION, resolver, RECORDER).execve(path, argv, envp);
}

INJECT_FUNCTION(execl);

extern "C" EXPORT_SYMBOL int HOOK_NAME(execlp)(const char* file, const char* arg, ...) {

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
    ct::Resolver resolver;
    INFO("hooked execlp called: file={}, argv[0]={}", file, argv[0]);
    return ct::Executor(LINKER, SESSION, resolver, RECORDER).execvpe(file, argv, envp);
}

INJECT_FUNCTION(execlp);

// int execle(const char *path, const char *arg, ..., char * const envp[]);
extern "C" EXPORT_SYMBOL int HOOK_NAME(execle)(const char* path, const char* arg, ...) {

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

    ct::Resolver resolver;
    INFO("hooked execle called: path={}, argv[0]={}", path, argv[0]);
    return ct::Executor(LINKER, SESSION, resolver, RECORDER).execve(path, argv, envp);
}

INJECT_FUNCTION(execle);

#pragma GCC diagnostic pop

extern "C" EXPORT_SYMBOL int HOOK_NAME(posix_spawn)(pid_t* pid,
                                                    const char* path,
                                                    const posix_spawn_file_actions_t* file_actions,
                                                    const posix_spawnattr_t* attrp,
                                                    char* const argv[],
                                                    char* const envp[]) {
    ct::Resolver resolver;
    INFO("hooked posix_spawn called: path={}, argv[0]={}", path, argv[0]);
    return ct::Executor(LINKER, SESSION, resolver, RECORDER)
        .posix_spawn(pid, path, file_actions, attrp, argv, envp);
}

INJECT_FUNCTION(posix_spawn);

extern "C" EXPORT_SYMBOL int HOOK_NAME(posix_spawnp)(pid_t* pid,
                                                     const char* file,
                                                     const posix_spawn_file_actions_t* file_actions,
                                                     const posix_spawnattr_t* attrp,
                                                     char* const argv[],
                                                     char* const envp[]) {
    ct::Resolver resolver;
    INFO("hooked posix_spawnp called: file={}, argv[0]={}", file, argv[0]);
    return ct::Executor(LINKER, SESSION, resolver, RECORDER)
        .posix_spawnp(pid, file, file_actions, attrp, argv, envp);
}

INJECT_FUNCTION(posix_spawnp);

