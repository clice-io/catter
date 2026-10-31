#include "support/paths.h"

#include <format>
#include <string>
#include <string_view>
#include <cpptrace/exceptions.hpp>
#include <kotatsu/fs/path.h>
#include <kotatsu/sys/dirs.h>
#include <kotatsu/sys/env.h>
#include <kotatsu/sys/process.h>

namespace catter {

std::string data_path(std::string_view name) {
    std::string path;
    if(kotatsu::sys::get_env(path, "CATTER_HOME") && !path.empty()) {
        // The build's processes start in other directories, where a relative
        // one names another.
        if(!kotatsu::fs::path::is_absolute(path)) {
            throw cpptrace::runtime_error(
                std::format("CATTER_HOME must be an absolute path, not `{}`", path));
        }
    } else {
#ifdef CATTER_WINDOWS
        if(auto error = kotatsu::sys::data_dir(path)) {
            throw cpptrace::runtime_error(
                std::format("Failed to find the local application data: {}", error.message()));
        }
        kotatsu::fs::path::push(path, "catter");
#else
        if(auto error = kotatsu::sys::home_dir(path)) {
            throw cpptrace::runtime_error(
                std::format("Failed to find the home directory: {}", error.message()));
        }
        kotatsu::fs::path::push(path, ".catter");
#endif
    }
    kotatsu::fs::path::push(path, name);
    return path;
}

std::string program_path(std::string_view name) {
    std::string path;
    if(auto error = kotatsu::sys::executable_path(path)) {
        throw cpptrace::runtime_error(
            std::format("Failed to find this program's executable: {}", error.message()));
    }
    kotatsu::fs::path::pop(path);
    kotatsu::fs::path::push(path, name);
    return path;
}

}  // namespace catter
