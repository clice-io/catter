#pragma once

#include <cstdint>
#include <system_error>
#include <unistd.h>
#include <stdint.h>

#ifdef CATTER_LINUX
#include <sys/syscall.h>
#include <pthread.h>

static pid_t get_thread_id(void) {
    return (pid_t)syscall(SYS_gettid);
}
#elif CATTER_MAC
#include <pthread.h>

static uint64_t get_thread_id(void) {
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

static const char** environment() noexcept {
#if CATTER_MAC
    return const_cast<const char**>(*_NSGetEnviron());
#elif CATTER_LINUX
    return const_cast<const char**>(environ);
#else
#error "Unsupported platform"
#endif
}

#include <string>
#include <vector>

#ifdef CATTER_LINUX
#include <unistd.h>
#elif CATTER_MAC
#include <mach-o/dyld.h>
#else
#error "Unsupported platform"
#endif

inline std::string get_executable_path(std::error_code& ec) {
    std::array<char, PATH_MAX> buf;
#ifdef CATTER_LINUX
    ssize_t len = readlink("/proc/self/exe", buf.data(), buf.size() - 1);
    if(len <= 0) {
        ec = std::error_code(errno, std::generic_category());
        return {};
    }
    buf[len] = '\0';
#elif defined(__APPLE__)
    uint32_t size = buf.size();
    if(_NSGetExecutablePath(buf.data(), &size) != 0) {
        ec = std::error_code(errno, std::generic_category());
        return {};
    }
#endif
    return std::string(buf.data());
}

inline std::string get_executable_path() {
    std::array<char, PATH_MAX> buf;
#ifdef CATTER_LINUX
    ssize_t len = readlink("/proc/self/exe", buf.data(), buf.size() - 1);
    if(len <= 0) {
        return {};
    }
    buf[len] = '\0';
#elif defined(__APPLE__)
    uint32_t size = buf.size();
    if(_NSGetExecutablePath(buf.data(), &size) != 0) {
        return {};
    }
#endif
    return std::string(buf.data());
}

#ifdef CATTER_MAC
#ifndef DYLD_INTERPOSE
#define DYLD_INTERPOSE(_replacement, _replacee)                                                    \
    __attribute__((used)) static struct {                                                          \
        const void* replacement;                                                                   \
        const void* replacee;                                                                      \
    } _interpose_##_replacee __attribute__((section("__DATA,__interpose"))) = {                    \
        (const void*)(unsigned long)&_replacement,                                                 \
        (const void*)(unsigned long)&_replacee};
#endif
#elif CATTER_LINUX
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
