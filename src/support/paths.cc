#include "support/paths.h"

#include <format>
#include <string>
#include <string_view>
#include <cpptrace/exceptions.hpp>
#include <kota/fs/path.h>
#include <kota/sys/dirs.h>
#include <kota/sys/env.h>
#include <kota/sys/process.h>

namespace catter {

std::string data_path(std::string_view name) {
    std::string path;
    if(kota::sys::get_env(path, "CATTER_HOME") && !path.empty()) {
        // The build's processes start in other directories, where a relative
        // one names another.
        if(!kota::fs::path::is_absolute(path)) {
            throw cpptrace::runtime_error(
                std::format("CATTER_HOME must be an absolute path, not `{}`", path));
        }
    } else {
#ifdef CATTER_WINDOWS
        if(auto error = kota::sys::data_dir(path)) {
            throw cpptrace::runtime_error(
                std::format("Failed to find the local application data: {}", error.message()));
        }
        kota::fs::path::push(path, "catter");
#else
        if(auto error = kota::sys::home_dir(path)) {
            throw cpptrace::runtime_error(
                std::format("Failed to find the home directory: {}", error.message()));
        }
        kota::fs::path::push(path, ".catter");
#endif
    }
    kota::fs::path::push(path, name);
    return path;
}

std::string pipe_name() {
#ifdef CATTER_WINDOWS
    return std::format(R"(\\.\pipe\catter-ipc-{})", kota::sys::pid());
#else
    return data_path(std::format("ipc-{}.sock", kota::sys::pid()));
#endif
}

std::string program_path(std::string_view name) {
    std::string path;
    if(auto error = kota::sys::executable_path(path)) {
        throw cpptrace::runtime_error(
            std::format("Failed to find this program's executable: {}", error.message()));
    }
    kota::fs::path::pop(path);
    kota::fs::path::push(path, name);
    return path;
}

}  // namespace catter
