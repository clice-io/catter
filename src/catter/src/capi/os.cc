#include <print>
#include "../apitool.h"
#include "libconfig/common.h"

namespace {
CAPI(os_name, ()->std::string) {
#ifdef __linux__
    return "linux";
#elif defined(__APPLE__) && defined(__MACH__)
    return "macos";
#elif defined(_WIN32) || defined(_WIN64)
    return "windows";
#else
#error "not support os"
#endif
}

CAPI(os_arch, ()->std::string) {
#if defined(__i386__) || defined(_M_IX86)
    return "x86";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x64";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#else
#error "not support architecture"
#endif
}

}  // namespace
