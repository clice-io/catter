#ifndef CATTER_WINDOWS

#include "shared/resolver.h"

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <ranges>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace catter::hook::shared::resolver {
namespace {

constexpr char k_dir_separator = '/';
constexpr char k_path_separator = ':';

bool contains_dir_separator(std::string_view candidate) {
    return candidate.contains(k_dir_separator);
}

const char* get_env_value(const char** envp, std::string_view key) {
    if(envp == nullptr) {
        return nullptr;
    }
    for(size_t i = 0; envp[i] != nullptr; ++i) {
        std::string_view entry(envp[i]);
        auto pos = entry.find('=');
        if(pos == std::string_view::npos) {
            continue;
        }
        if(entry.substr(0, pos) == key) {
            return envp[i] + pos + 1;
        }
    }
    return nullptr;
}

}  // namespace

std::expected<std::filesystem::path, int> resolve_path_like(std::string_view file) {
    std::filesystem::path path(file);
    if(!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
        return std::unexpected(ENOENT);
    }
    if(::access(path.c_str(), X_OK) == 0) {
        return path;
    }
    return std::unexpected(errno);
}

std::expected<std::filesystem::path, int> resolve_from_search_path(std::string_view file,
                                                                   const char* search_path) {
    if(contains_dir_separator(file)) {
        return resolve_path_like(file);
    }
    if(search_path == nullptr) {
        return std::unexpected(ENOENT);
    }

    for(const auto& path: std::views::split(std::string_view(search_path), k_path_separator)) {
        if(path.empty()) {
            continue;
        }
        std::filesystem::path candidate(path.begin(), path.end());
        candidate /= file;
        if(auto resolved = resolve_path_like(candidate.string()); resolved.has_value()) {
            return resolved;
        }
    }
    return std::unexpected(ENOENT);
}

std::expected<std::filesystem::path, int> resolve_from_path_env(std::string_view file,
                                                                const char** envp) {
    if(contains_dir_separator(file)) {
        return resolve_path_like(file);
    }

    if(const char* path_env = get_env_value(envp, "PATH"); path_env != nullptr) {
        return resolve_from_search_path(file, path_env);
    }

    if(const char* path_env = std::getenv("PATH"); path_env != nullptr) {
        return resolve_from_search_path(file, path_env);
    }

    const size_t search_path_length = ::confstr(_CS_PATH, nullptr, 0);
    if(search_path_length != 0) {
        std::string search_path(search_path_length - 1, '\0');
        if(::confstr(_CS_PATH, search_path.data(), search_path_length) != 0) {
            return resolve_from_search_path(file, search_path.data());
        }
    }

    return std::unexpected(ENOENT);
}

}  // namespace catter::hook::shared::resolver

#endif
