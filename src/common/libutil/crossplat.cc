#include "crossplat.h"
#include <vector>
#include <string>
#ifdef CATTER_LINUX
extern char** environ;
#endif
#if CATTER_MAC
#include <crt_externs.h>
#endif

namespace catter::util {
std::vector<std::string> get_environment() noexcept {
    std::vector<std::string> env_vars;
#if defined(CATTER_MAC)
    auto envp = const_cast<const char**>(*_NSGetEnviron());
    for(int i = 0; envp[i] != nullptr; ++i) {
        env_vars.emplace_back(envp[i]);
    }
#elif defined(CATTER_LINUX)
    for(int i = 0; environ[i] != nullptr; ++i) {
        env_vars.emplace_back(environ[i]);
    }
#elif defined(CATTER_WINDOWS)
// TODO: windows environment variable
#else
#error "Unsupported platform"
#endif
    return env_vars;
}
}  // namespace catter::util
