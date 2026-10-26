#pragma once

#include <cstdint>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>

#ifdef CATTER_LINUX
#include <sys/syscall.h>

inline pid_t get_thread_id(void) {
    return (pid_t)syscall(SYS_gettid);
}
#elif defined(CATTER_MAC)

inline uint64_t get_thread_id(void) {
    uint64_t tid;
    pthread_threadid_np(NULL, &tid);
    return tid;
}
#else
#error "Unsupported platform"
#endif

#if CATTER_LINUX
extern char** environ;
#endif
#if CATTER_MAC
#include <crt_externs.h>
#endif

inline char** environment() noexcept {
#if defined(CATTER_MAC)
    return *_NSGetEnviron();
#elif defined(CATTER_LINUX)
    return environ;
#else
#error "Unsupported platform"
#endif
}

#if defined(CATTER_MAC)
#ifndef DYLD_INTERPOSE
#define DYLD_INTERPOSE(_replacement, _replacee)                                                    \
    __attribute__((used)) static struct {                                                          \
        const void* replacement;                                                                   \
        const void* replacee;                                                                      \
    } _interpose_##_replacee __attribute__((section("__DATA,__interpose"))) = {                    \
        (const void*)(unsigned long)&_replacement,                                                 \
        (const void*)(unsigned long)&_replacee};
#endif
#elif defined(CATTER_LINUX)
#define DYLD_INTERPOSE(_replacement, _replacee)
#else
#error "Unsupported platform"
#endif

// if macos, our hook function use a prefix of catter_mac_
#ifdef CATTER_MAC
#define HOOK_NAME(fn) catter_mac_##fn
#else
#define HOOK_NAME(fn) fn
#endif

#define INJECT_FUNCTION(fn) DYLD_INTERPOSE(HOOK_NAME(fn), fn)

template <typename T>
T dynamic_linker(const char* const name) noexcept {
    return reinterpret_cast<T>(dlsym(RTLD_NEXT, name));
}
