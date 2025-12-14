#include "resolver.h"

#include "array.h"
#include "environment.h"
#include "config/linux-mac-hook.h"
#include "paths.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <expected>
#include <unistd.h>
#include <sys/stat.h>

namespace {

bool contains_dir_separator(const std::string_view& candidate) {
    return std::find(candidate.begin(), candidate.end(), '/') != candidate.end();
}
}  // namespace

namespace catter {

Resolver::Resolver() noexcept : result_() {
    result_[0] = 0;
}

std::expected<const char*, int> Resolver::from_current_directory(const std::string_view& file) {
    // copy the input to result.
    array::copy(file.begin(), file.end() + 1, result_, result_ + PATH_MAX);
    // check if this is a file
    struct stat sb{};
    ::stat(result_, &sb);
    if((sb.st_mode & S_IFMT) != S_IFREG) {
        return std::unexpected(ENOENT);
    }
    // check if it's okay to execute.
    if(0 == ::access(result_, X_OK)) {
        const char* ptr = result_;
        return ptr;
    }
    // try to set a meaningful error value.
    if(0 == ::access(result_, F_OK)) {
        return std::unexpected(EACCES);
    }
    return std::unexpected(ENOENT);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvla"

std::expected<const char*, int> Resolver::from_path(std::string_view const& file,
                                                    const char** envp) {
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
            char search_path[search_path_length];
            if(::confstr(_CS_PATH, search_path, search_path_length) != 0) {
                return from_search_path(file, search_path);
            }
        }
        return std::unexpected(ENOENT);
    }
}

#pragma GCC diagnostic pop

std::expected<const char*, int> Resolver::from_search_path(std::string_view const& file,
                                                           const char* search_path) {
    if(contains_dir_separator(file)) {
        // the file contains a dir separator, it is treated as path.
        return from_current_directory(file);
    } else {
        // otherwise use the given search path to locate the executable.
        for(const auto& path: Paths(search_path)) {
            // ignore empty entries
            if(path.empty()) {
                continue;
            }
            // check if it's possible to assemble a PATH
            if((file.size() + path.size() + 2) > PATH_MAX) {
                continue;
            }
            // create a path
            char candidate[PATH_MAX];
            {
                char* const candidate_begin = candidate;
                char* const candidate_end = candidate + PATH_MAX;
                auto it = array::copy(path.begin(), path.end(), candidate_begin, candidate_end);
                *it = config::OS_DIR_SEPARATOR;
                it++;
                it = array::copy(file.begin(), file.end(), it, candidate_end);
                *it = 0;
            }
            // check if it's okay to execute.
            if(auto result = from_current_directory(candidate); result.has_value()) {
                return result;
            }
        }
        // if all attempt were failing, then quit with a failure.
        return std::unexpected(ENOENT);
    }
}
}  // namespace catter
