#include "support/program.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <kotatsu/fs/path.h>
#include <kotatsu/sys/process.h>

#ifdef CATTER_WINDOWS
#include <kotatsu/sys/windows.h>
#endif

namespace catter {

std::string resolve_program(std::string_view name, std::string_view directory) {
    std::string program(name);
    // A name that is a path, rather than looked for, is relative to the
    // directory.
    if(!directory.empty() &&
       std::ranges::any_of(name, [](char c) { return kotatsu::fs::path::is_separator(c); })) {
        program = directory;
        kotatsu::fs::path::push(program, name);
    }
    std::string path;
#ifdef CATTER_WINDOWS
    auto directories = kotatsu::sys::windows::create_process_directories();
    // CreateProcess looks in the current directory second.
    if(directories && !directory.empty()) {
        (*directories)[1] = directory;
    }
    if(!directories ||
       kotatsu::sys::windows::find_create_process_program(path, program, *directories)) {
        path = name;
    }
#else
    if(kotatsu::sys::find_program(path, program)) {
        path = name;
    }
#endif
    return path;
}

}  // namespace catter
