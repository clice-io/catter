#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <string>
#include <dlfcn.h>
#include <spawn.h>
#include <unistd.h>
#include <kota/sys/process.h>

#include "hook/unix/debug.h"
#include "hook/unix/environment.h"
#include "hook/unix/executor.h"

#ifdef DEBUG
#include "support/messages.h"
#include "support/paths.h"
#endif

#define EXPORT_SYMBOL __attribute__((visibility("default")))

// On Linux the library exports the functions it hooks (exports.map), which the
// loader binds the program's calls to; on macOS it interposes them, under names
// of its own.
#ifdef CATTER_MAC
#define HOOK_NAME(fn) catter_mac_##fn
#define INJECT_FUNCTION(fn)                                                                        \
    __attribute__((used)) static struct {                                                          \
        const void* replacement;                                                                   \
        const void* replacee;                                                                      \
    } _interpose_##fn __attribute__((section("__DATA,__interpose"))) = {                           \
        (const void*)(unsigned long)&HOOK_NAME(fn),                                                \
        (const void*)(unsigned long)&fn};
#else
#define HOOK_NAME(fn) fn
#define INJECT_FUNCTION(fn)
#endif

namespace {

std::atomic<bool> loaded(false);

catter::hook::Executor executor;

}  // namespace

// glibc gives a constructor argc and argv.
extern "C" EXPORT_SYMBOL void on_load(int argc, char* argv[]) __attribute__((constructor));

extern "C" EXPORT_SYMBOL void on_load([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    if(loaded.exchange(true)) {
        return;
    }
#ifdef DEBUG
    // Before the logger starts, a log goes to stdout, the program's.
    if(auto ipc = std::getenv(catter::env::IPC)) {
        catter::log::init_logger("catter-hook",
                                 catter::data_path(ipc, "log/catter-hook.log"),
                                 true);
    } else {
        catter::log::mute_logger();
    }
    std::string program;
    kota::sys::executable_path(program);
    INFO("catter hook library loaded, from executable path: {}", program);
#endif
#ifdef CATTER_MAC
    executor.init(catter::hook::environment(), &::execve, &::posix_spawn);
#else
    executor.init(catter::hook::environment(),
                  reinterpret_cast<catter::hook::ExecveFn*>(dlsym(RTLD_NEXT, "execve")),
                  reinterpret_cast<catter::hook::PosixSpawnFn*>(dlsym(RTLD_NEXT, "posix_spawn")));
    executor.restart_undecided(argc, argv);
#endif
    errno = 0;
}

extern "C" EXPORT_SYMBOL int HOOK_NAME(execve)(const char* path,
                                               char* const argv[],
                                               char* const envp[]) {
    return executor.execve(path, argv, envp);
}

INJECT_FUNCTION(execve);

extern "C" EXPORT_SYMBOL int HOOK_NAME(execv)(const char* path, char* const argv[]) {
    return executor.execv(path, argv);
}

INJECT_FUNCTION(execv);

#ifdef CATTER_LINUX
extern "C" EXPORT_SYMBOL int HOOK_NAME(execvpe)(const char* file,
                                                char* const argv[],
                                                char* const envp[]) {
    return executor.execvpe(file, argv, envp);
}
#endif

extern "C" EXPORT_SYMBOL int HOOK_NAME(execvp)(const char* file, char* const argv[]) {
    return executor.execvp(file, argv);
}

INJECT_FUNCTION(execvp);

#ifdef CATTER_MAC
extern "C" EXPORT_SYMBOL int HOOK_NAME(execvP)(const char* file,
                                               const char* search_path,
                                               char* const argv[]) {
    return executor.execvP(file, search_path, argv, catter::hook::environment());
}

INJECT_FUNCTION(execvP);
#endif

extern "C" EXPORT_SYMBOL int HOOK_NAME(execl)(const char* path, const char* arg, ...) {
    va_list ap;
    va_start(ap, arg);
    auto result = executor.execl(path, arg, &ap);
    va_end(ap);
    return result;
}

INJECT_FUNCTION(execl);

extern "C" EXPORT_SYMBOL int HOOK_NAME(execlp)(const char* file, const char* arg, ...) {
    va_list ap;
    va_start(ap, arg);
    auto result = executor.execlp(file, arg, &ap);
    va_end(ap);
    return result;
}

INJECT_FUNCTION(execlp);

extern "C" EXPORT_SYMBOL int HOOK_NAME(execle)(const char* path, const char* arg, ...) {
    va_list ap;
    va_start(ap, arg);
    auto result = executor.execle(path, arg, &ap);
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
    return executor.posix_spawn(pid, path, file_actions, attrp, argv, envp);
}

INJECT_FUNCTION(posix_spawn);

extern "C" EXPORT_SYMBOL int HOOK_NAME(posix_spawnp)(pid_t* pid,
                                                     const char* file,
                                                     const posix_spawn_file_actions_t* file_actions,
                                                     const posix_spawnattr_t* attrp,
                                                     char* const argv[],
                                                     char* const envp[]) {
    return executor.posix_spawnp(pid, file, file_actions, attrp, argv, envp);
}

INJECT_FUNCTION(posix_spawnp);
