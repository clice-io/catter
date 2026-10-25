#include "support/crossplat.h"

#include <array>
#include <cerrno>
#include <climits>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <system_error>
#include <vector>
#include <cpptrace/exceptions.hpp>

#include "support/encoding.h"

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

static std::filesystem::path default_catter_data_path() {
    const char* home = getenv("HOME");
    if(home == nullptr) {
        throw cpptrace::runtime_error("HOME environment variable not set");
    }
    std::filesystem::path path = home;
    return path / ".catter";
}

static std::optional<std::filesystem::path> catter_home() {
    const char* home = getenv("CATTER_HOME");
    if(home == nullptr || *home == '\0') {
        return std::nullopt;
    }
    return std::filesystem::path(home);
}

std::filesystem::path get_executable_path() {
    std::array<char, 1024> buf;
    ssize_t len = readlink("/proc/self/exe", buf.data(), buf.size() - 1);
    if(len <= 0) {
        auto err = std::error_code(errno, std::generic_category());
        throw cpptrace::runtime_error(
            std::format("readlink failed with code {}: {})", err.value(), err.message()));
    }
    buf[len] = '\0';
    return std::filesystem::path(buf.data());
}

}  // namespace catter::util

#elif defined(CATTER_MAC)

#include <crt_externs.h>
#include <mach-o/dyld.h>

namespace catter::util {
std::vector<std::string> get_environment() noexcept {
    std::vector<std::string> env_vars;
    auto envp = *_NSGetEnviron();
    for(int i = 0; envp[i] != nullptr; ++i) {
        env_vars.emplace_back(envp[i]);
    }
    return env_vars;
}

static std::filesystem::path default_catter_data_path() {
    const char* home = getenv("HOME");
    if(home == nullptr) {
        throw cpptrace::runtime_error("HOME environment variable not set");
    }
    std::filesystem::path path = home;
    return path / ".catter";
}

static std::optional<std::filesystem::path> catter_home() {
    const char* home = getenv("CATTER_HOME");
    if(home == nullptr || *home == '\0') {
        return std::nullopt;
    }
    return std::filesystem::path(home);
}

std::filesystem::path get_executable_path() {
    std::array<char, 1024> buf;
    uint32_t size = buf.size();
    if(_NSGetExecutablePath(buf.data(), &size) != 0) {
        auto err = std::error_code(ERANGE, std::generic_category());
        throw cpptrace::runtime_error(
            std::format("readlink failed with code {}: {})", err.value(), err.message()));
    }
    return std::filesystem::path(buf.data());
}

}  // namespace catter::util

#elif defined(CATTER_WINDOWS)

#include <ShlObj.h>
#include <windows.h>

namespace catter::util {
std::vector<std::string> get_environment() noexcept {
    auto env_block = GetEnvironmentStringsW();

    if(env_block == nullptr) {
        return {};
    }

    std::vector<std::string> env_vars;

    for(auto current = env_block; *current != L'\0';) {
        std::wstring_view sv(current);
        env_vars.emplace_back(from_wide(sv));
        std::advance(current, sv.size() + 1);
    }

    FreeEnvironmentStringsW(env_block);
    return env_vars;
}

std::filesystem::path get_executable_path() {
    std::vector<wchar_t> data;
    data.resize(MAX_PATH);
    while(true) {
        if(GetModuleFileNameW(nullptr, data.data(), static_cast<DWORD>(data.size())) ==
               data.size() &&
           GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            data.resize(data.size() * 2);
        } else {
            break;
        }
    }

    return std::filesystem::path(data.data());
}

static std::optional<std::filesystem::path> catter_home() {
    const wchar_t* home = _wgetenv(L"CATTER_HOME");
    if(home == nullptr || *home == L'\0') {
        return std::nullopt;
    }
    return std::filesystem::path(home);
}

static std::filesystem::path default_catter_data_path() {
    PWSTR path = nullptr;

    HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &path);

    if(FAILED(hr))
        throw cpptrace::runtime_error("SHGetKnownFolderPath failed");

    std::filesystem::path result(path);

    CoTaskMemFree(path);

    return result / "catter";
}

}  // namespace catter::util
#endif

namespace catter::util {
std::filesystem::path get_catter_root_path() {
    return get_executable_path().parent_path();
};

std::filesystem::path get_catter_data_path() {
    if(auto home = catter_home()) {
        // The build's processes start in other directories, where a relative
        // one names another.
        if(home->is_relative()) {
            throw cpptrace::runtime_error(
                std::format("CATTER_HOME must be an absolute path, not `{}`", to_utf8(*home)));
        }
        return *home;
    }
    return default_catter_data_path();
}
}  // namespace catter::util
