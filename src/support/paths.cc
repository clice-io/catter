#include "support/paths.h"

#include <algorithm>
#include <format>
#include <string>
#include <string_view>
#include <cpptrace/exceptions.hpp>
#include <kota/fs/path.h>
#include <kota/sys/dirs.h>
#include <kota/sys/env.h>
#include <kota/sys/process.h>

#ifdef CATTER_WINDOWS
#include <kota/sys/windows.h>
#endif

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

std::string resolve_program(std::string_view name, std::string_view directory) {
    std::string program(name);
    // A name that is a path, rather than looked for, is relative to the
    // directory.
    if(!directory.empty() &&
       std::ranges::any_of(name, [](char c) { return kota::fs::path::is_separator(c); })) {
        program = directory;
        kota::fs::path::push(program, name);
    }
    std::string path;
#ifdef CATTER_WINDOWS
    auto directories = kota::sys::windows::create_process_directories();
    // CreateProcess looks in the current directory second.
    if(directories && !directory.empty()) {
        (*directories)[1] = directory;
    }
    if(!directories ||
       kota::sys::windows::find_create_process_program(path, program, *directories)) {
        path = name;
    }
#else
    if(kota::sys::find_program(path, program)) {
        path = name;
    }
#endif
    return path;
}

}  // namespace catter
