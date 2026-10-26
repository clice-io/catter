#include "hook/windows/util.h"

#include <string>
#include <string_view>
#include <kota/support/utf16.h>
#include <kota/fs/resolve.h>
#include <kota/sys/env.h>
#include <kota/sys/windows.h>

#include "support/config/hook_windows.h"

namespace catter::hook {

std::string resolve_program(const wchar_t* application_name, const wchar_t* command_line) {
    std::string found;
    if(application_name != nullptr && *application_name != L'\0') {
        kota::from_utf16(found, application_name);
    } else {
        std::string line;
        if(command_line != nullptr) {
            kota::from_utf16(line, command_line);
        }
        auto words = kota::sys::windows::split_command_line(line);
        auto first = words.begin();
        if(first == words.end()) {
            return {};
        }
        std::string name(*first);
        auto directories = kota::sys::windows::create_process_directories();
        if(!directories ||
           kota::sys::windows::find_create_process_program(found, name, *directories)) {
            return name;
        }
    }
    // Relative to this process's current directory, which the child's need
    // not be.
    std::string path;
    return kota::fs::absolute(path, found) ? found : path;
}

std::wstring proxy_command_line(const wchar_t* application_name, const wchar_t* command_line) {
    std::string proxy;
    std::string id;
    std::string pipe;
    kota::sys::get_env(proxy, config::ENV_VAR_PROXY_PATH);
    kota::sys::get_env(id, config::ENV_VAR_IPC_ID);
    kota::sys::get_env(pipe, config::ENV_VAR_IPC_PIPE);
    const auto program = resolve_program(application_name, command_line);

    std::string line;
    kota::sys::windows::join_command_line(
        line,
        {proxy, "-p", id, "--exec", program, "--ipc", pipe, "--"});
    line += ' ';
    std::wstring wide;
    kota::to_utf16(wide, line);
    if(command_line != nullptr) {
        wide += command_line;
    }
    return wide;
}

}  // namespace catter::hook
