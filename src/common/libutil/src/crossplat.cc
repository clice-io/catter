#include "libutil/crossplat.h"
#include <filesystem>
#include <array>
#include <climits>
#include <system_error>
#include <vector>
#include <string>

#if defined(CATTER_LINUX)
#include <unistd.h>
extern char** environ;

namespace catter::util {
std::vector<std::string> get_environment() noexcept {
    std::vector<std::string> env_vars;
    for(int i = 0; environ[i] != nullptr; ++i) {
        env_vars.emplace_back(environ[i]);
    }
    return env_vars;
}

std::filesystem::path get_log_path() {
    const char* home = getenv("HOME");
    if(home == nullptr) {
        throw std::runtime_error("HOME environment variable not set");
    }
    std::filesystem::path path = home;
    return path / ".catter";
}

std::filesystem::path get_executable_path(std::error_code& ec) {
    std::array<char, 1024> buf;
    ssize_t len = readlink("/proc/self/exe", buf.data(), buf.size() - 1);
    if(len <= 0) {
        ec = std::error_code(errno, std::generic_category());
        return {};
    }
    buf[len] = '\0';
    return std::filesystem::path(buf.data());
}

}  // namespace catter::util

#elif defined(CATTER_MAC)

#include <unistd.h>
#include <crt_externs.h>

namespace catter::util {
std::vector<std::string> get_environment() noexcept {
    std::vector<std::string> env_vars;
    auto envp = const_cast<const char**>(*_NSGetEnviron());
    for(int i = 0; envp[i] != nullptr; ++i) {
        env_vars.emplace_back(envp[i]);
    }
    return env_vars;
}

std::filesystem::path get_log_path() {
    const char* home = getenv("HOME");
    if(home == nullptr) {
        throw std::runtime_error("HOME environment variable not set");
    }
    std::filesystem::path path = home;
    return path / ".catter";
}

std::filesystem::path get_executable_path(std::error_code& ec) {
    std::array<char, 1024> buf;
    uint32_t size = buf.size();
    if(_NSGetExecutablePath(buf.data(), &size) != 0) {
        ec = std::error_code(ERANGE, std::generic_category());
        return {};
    }
    return std::filesystem::path(buf.data());
}

}  // namespace catter::util

#elif defined(CATTER_WINDOWS)

#include <windows.h>

namespace catter::util {
std::vector<std::string> get_environment() noexcept {
    return {};
}

std::filesystem::path get_executable_path() {
    std::vector<char> data;
    data.resize(MAX_PATH);
    while(true) {
        if(GetModuleFileNameA(nullptr, data.data(), data.size()) == data.size() &&
           GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            data.resize(data.size() * 2);
        } else {
            break;
        }
    }

    return std::filesystem::path(data.data());
}

std::filesystem::path get_log_path() {
    return get_catter_root_path() / "logs";
}

}  // namespace catter::util
#endif

namespace catter::util {
std::filesystem::path get_catter_root_path() {
    return get_executable_path().parent_path();
};
}  // namespace catter::util
