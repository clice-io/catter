#include "shared/resolver.h"

#ifndef CATTER_WINDOWS

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <limits.h>
#include <unistd.h>

namespace catter::hook::shared {
namespace {

bool contains_dir_separator(std::string_view candidate) {
    return candidate.contains('/');
}

const char* get_env_value(const char** envp, std::string_view key) noexcept {
    if(envp == nullptr) {
        return nullptr;
    }
    for(size_t i = 0; envp[i] != nullptr; ++i) {
        std::string_view entry(envp[i]);
        auto separator = entry.find('=');
        if(separator == key.size() && entry.substr(0, separator) == key) {
            return envp[i] + separator + 1;
        }
    }
    return nullptr;
}

}  // namespace

namespace fs = std::filesystem;

std::expected<fs::path, int> resolve_path_like(std::string_view file) {
    fs::path path(file);
    if(!fs::exists(path) || !fs::is_regular_file(path)) {
        return std::unexpected(ENOENT);
    }
    if(::access(path.c_str(), X_OK) == 0) {
        return path;
    }
    return std::unexpected(errno);
}

std::expected<fs::path, int> resolve_from_environment(std::string_view file, const char** envp) {
    if(contains_dir_separator(file)) {
        return resolve_path_like(file);
    }
    if(const char* paths = get_env_value(envp, "PATH"); paths != nullptr) {
        return resolve_from_search_path(file, paths);
    }

    const size_t search_path_length = ::confstr(_CS_PATH, nullptr, 0);
    if(search_path_length != 0) {
        std::string search_path(search_path_length - 1, '\0');
        if(::confstr(_CS_PATH, search_path.data(), search_path_length) != 0) {
            return resolve_from_search_path(file, search_path);
        }
    }
    return std::unexpected(ENOENT);
}

std::expected<fs::path, int> resolve_from_search_path(std::string_view file,
                                                      std::string_view search_path) {
    if(contains_dir_separator(file)) {
        return resolve_path_like(file);
    }

    size_t start = 0;
    while(start <= search_path.size()) {
        auto separator = search_path.find(':', start);
        auto entry = search_path.substr(start,
                                        separator == std::string_view::npos ? search_path.size() -
                                                                                  start
                                                                            : separator - start);
        if(!entry.empty() && (file.size() + entry.size() + 2) <= PATH_MAX) {
            fs::path candidate(entry.begin(), entry.end());
            candidate /= file;
            if(auto resolved = resolve_path_like(candidate.string()); resolved.has_value()) {
                return resolved;
            }
        }

        if(separator == std::string_view::npos) {
            break;
        }
        start = separator + 1;
    }
    return std::unexpected(ENOENT);
}

}  // namespace catter::hook::shared

#endif
