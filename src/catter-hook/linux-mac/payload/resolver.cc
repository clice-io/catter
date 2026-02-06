#include "resolver.h"

#include "environment.h"
#include "linux-mac/config.h"

#include <cerrno>
#include <ranges>
#include <cstdlib>
#include <expected>
#include <unistd.h>
#include <filesystem>
#include <sys/stat.h>

namespace {

bool contains_dir_separator(const std::string_view& candidate) {
    return candidate.contains(catter::config::OS_DIR_SEPARATOR);
}
}  // namespace

namespace fs = std::filesystem;

namespace catter {
std::expected<fs::path, int> Resolver::from_current_directory(std::string_view file) const {
    fs::path p(file);
    if(!fs::exists(p) || !fs::is_regular_file(p)) {
        return std::unexpected(ENOENT);
    }
    if(::access(p.c_str(), X_OK) == 0) {
        return p;
    }
    return std::unexpected(errno);
}

std::expected<fs::path, int> Resolver::from_path(std::string_view file, const char** envp) const {
    if(contains_dir_separator(file)) {
        // the file contains a dir separator, it is treated as path.
        return from_current_directory(file);
    } else {
        // otherwise use the PATH variable to locate the executable.
        const char* const paths = env::get_env_value(envp, "PATH");
        if(paths != nullptr) {
            return from_search_path(file, paths);
        }
        // fall back to `confstr` PATH value if the environment has no value.
        const size_t search_path_length = ::confstr(_CS_PATH, nullptr, 0);
        if(search_path_length != 0) {
            std::string search_path(search_path_length - 1, '\0');
            if(::confstr(_CS_PATH, search_path.data(), search_path_length) != 0) {
                return from_search_path(file, search_path.data());
            }
        }
        return std::unexpected(ENOENT);
    }
}

std::expected<fs::path, int> Resolver::from_search_path(std::string_view file,
                                                        const char* search_path) const {
    if(contains_dir_separator(file)) {
        // the file contains a dir separator, it is treated as path.
        return from_current_directory(file);
    } else {
        // otherwise use the given search path to locate the executable.
        for(const auto& path:
            std::views::split(std::string_view(search_path), config::OS_PATH_SEPARATOR)) {
            // ignore empty entries
            if(path.empty()) {
                continue;
            }
            // check if it's possible to assemble a PATH
            if((file.size() + path.size() + 2) > PATH_MAX) {
                continue;
            }
            // create a path
            fs::path candidate(path.begin(), path.end());
            candidate /= file;
            // check if it's okay to execute.
            if(auto result = from_current_directory(candidate.c_str()); result.has_value()) {
                return result;
            }
        }
        // if all attempt were failing, then quit with a failure.
        return std::unexpected(ENOENT);
    }
}
}  // namespace catter
