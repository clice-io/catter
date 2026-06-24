#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <spawn.h>
#include <unistd.h>

#include "crossplat.h"
#include "debug.h"
#include "executor.h"
#include "unix/config.h"

#define EXPORT_SYMBOL __attribute__((visibility("default")))

namespace {

const char* safe_cstr(const char* value) noexcept {
    return value == nullptr ? "" : value;
}

const char* safe_argv0(const char* const argv[]) noexcept {
    return argv == nullptr ? "" : safe_cstr(argv[0]);
}

}  // namespace

/**
 * Library static data
 *
 * Will be initialized, when the library loaded into memory.
 */
namespace {

// This is used for being multi thread safe (loading time only).
std::atomic<bool> LOADED(false);
// These are related to the functionality of this library.
catter::Executor EXECUTOR;

}  // namespace

/**
 * Library entry point.
 *
 * The first method to call after the library is loaded into memory.
 */

extern "C" EXPORT_SYMBOL void on_load() __attribute__((constructor));

extern "C" EXPORT_SYMBOL void on_load() {
    // Test whether on_load was called already.
    if(LOADED.exchange(true))
        return;
#ifdef DEBUG
    auto path = catter::util::get_catter_data_path() / catter::config::hook::LOG_PATH_REL;
    catter::log::init_logger("catter-hook", path, false);
#endif
    INFO("catter hook library loaded, from executable path: {}", get_executable_path());
    EXECUTOR.init(environment());
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
    INFO("hooked execve called: path={}, argv[0]={}", safe_cstr(path), safe_argv0(argv));
    return EXECUTOR.execve(path, argv, envp);
}

INJECT_FUNCTION(execve);

extern "C" EXPORT_SYMBOL int HOOK_NAME(execv)(const char* path, char* const argv[]) {
    INFO("hooked execv called: path={}, argv[0]={}", safe_cstr(path), safe_argv0(argv));
    return EXECUTOR.execv(path, argv);
}

INJECT_FUNCTION(execv);

/// Mac do not have execvpe, we only hook it in linux
extern "C" EXPORT_SYMBOL int HOOK_NAME(execvpe)(const char* file,
                                                char* const argv[],
                                                char* const envp[]) {
    INFO("hooked execvpe called: file={}, argv[0]={}", safe_cstr(file), safe_argv0(argv));
    return EXECUTOR.execvpe(file, argv, envp);
}

// INJECT_FUNCTION(execvpe);

extern "C" EXPORT_SYMBOL int HOOK_NAME(execvp)(const char* file, char* const argv[]) {
    INFO("hooked execvp called: file={}, argv[0]={}", safe_cstr(file), safe_argv0(argv));
    return EXECUTOR.execvp(file, argv);
}

INJECT_FUNCTION(execvp);

extern "C" EXPORT_SYMBOL int HOOK_NAME(execvP)(const char* file,
                                               const char* search_path,
                                               char* const argv[]) {
    auto envp = environment();
    INFO("hooked execvP called: file={}, argv[0]={}", safe_cstr(file), safe_argv0(argv));
    return EXECUTOR.execvP(file, search_path, argv, envp);
}

INJECT_FUNCTION(execvP);

extern "C" EXPORT_SYMBOL int HOOK_NAME(exect)(const char* path,
                                              char* const argv[],
                                              char* const envp[]) {
    INFO("hooked exect called: path={}, argv[0]={}", safe_cstr(path), safe_argv0(argv));
    return EXECUTOR.exect(path, argv, envp);
}

// INJECT_FUNCTION(exect);

extern "C" EXPORT_SYMBOL int HOOK_NAME(execl)(const char* path, const char* arg, ...) {
    va_list ap;
    va_start(ap, arg);
    INFO("hooked execl called: path={}, argv[0]={}", safe_cstr(path), safe_cstr(arg));
    auto result = EXECUTOR.execl(path, arg, &ap);
    va_end(ap);
    return result;
}

INJECT_FUNCTION(execl);

extern "C" EXPORT_SYMBOL int HOOK_NAME(execlp)(const char* file, const char* arg, ...) {
    va_list ap;
    va_start(ap, arg);
    INFO("hooked execlp called: file={}, argv[0]={}", safe_cstr(file), safe_cstr(arg));
    auto result = EXECUTOR.execlp(file, arg, &ap);
    va_end(ap);
    return result;
}

INJECT_FUNCTION(execlp);

// int execle(const char *path, const char *arg, ..., char * const envp[]);
extern "C" EXPORT_SYMBOL int HOOK_NAME(execle)(const char* path, const char* arg, ...) {
    va_list ap;
    va_start(ap, arg);
    INFO("hooked execle called: path={}, argv[0]={}", safe_cstr(path), safe_cstr(arg));
    auto result = EXECUTOR.execle(path, arg, &ap);
    va_end(ap);
    return result;
}

INJECT_FUNCTION(execle);

extern "C" EXPORT_SYMBOL int HOOK_NAME(posix_spawn)(pid_t* pid,
                                                    const char* path,
                                                    const posix_spawn_file_actions_t* file_actions,
                                                    const posix_spawnattr_t* attrp,
                                                    char* const argv[],
                                                    char* const envp[]) {
    INFO("hooked posix_spawn called: path={}, argv[0]={}", safe_cstr(path), safe_argv0(argv));
    return EXECUTOR.posix_spawn(pid, path, file_actions, attrp, argv, envp);
}

INJECT_FUNCTION(posix_spawn);

extern "C" EXPORT_SYMBOL int HOOK_NAME(posix_spawnp)(pid_t* pid,
                                                     const char* file,
                                                     const posix_spawn_file_actions_t* file_actions,
                                                     const posix_spawnattr_t* attrp,
                                                     char* const argv[],
                                                     char* const envp[]) {
    INFO("hooked posix_spawnp called: file={}, argv[0]={}", safe_cstr(file), safe_argv0(argv));
    return EXECUTOR.posix_spawnp(pid, file, file_actions, attrp, argv, envp);
}

INJECT_FUNCTION(posix_spawnp);
