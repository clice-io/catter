#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <string_view>
#include <cpptrace/exceptions.hpp>
#include <kota/fs/status.h>
#if defined(CATTER_MAC)
#include <mach-o/utils.h>
#include <kota/fs/file.h>
#endif

#include "capture/inject/hook.h"
#include "session/data.h"
#include "session/process.h"
#include "support/config/catter-proxy.h"
#include "support/config/hook_unix.h"
#include "support/log.h"
#include "support/paths.h"

namespace catter {

#if defined(CATTER_MAC)
/// The architecture `executable` runs as, as dyld names it; a script runs as
/// its interpreter. Empty for what this Mac cannot run.
static std::string_view arch_of(std::string_view executable) {
    std::array<char, 512> head;
    std::size_t size = 0;
    if(auto file = kota::fs::File::open(executable)) {
        size = file->read(head).value_or(0);
    }
    std::string_view line(head.data(), size);
    std::string program(executable);
    if(line.starts_with("#!")) {
        line.remove_prefix(2);
        line.remove_prefix(std::min(line.find_first_not_of(" \t"), line.size()));
        program = line.substr(0, line.find_first_of(" \t\n"));
    }

    __block std::string_view arch;
    macho_best_slice(program.c_str(), ^(const mach_header* slice, uint64_t, size_t) {
        arch = macho_arch_name_for_mach_header(slice);
    });
    return arch;
}
#endif

kota::process::options hook_options(data::command command,
                                    data::ipcid_t id,
                                    std::string_view pipe_name) {
    LOG_INFO("new command id is: {}", id);

#if defined(CATTER_MAC)
    // dyld ends a program that cannot load a library inserted into it: Apple's
    // own tools, which run as arm64e, and Intel programs under Rosetta. Where
    // System Integrity Protection is on, dyld drops the hook from Apple's tools
    // all the same.
    if(auto arch = arch_of(command.executable);
       !arch.empty() && arch != macho_arch_name_for_mach_header(nullptr)) {
        LOG_WARN("No hook for {}: {} runs, but what it starts is not captured",
                 arch,
                 command.executable);
        return process_options(command);
    }
#endif

    const auto lib_path = program_path(catter::config::RELATIVE_PATH_OF_HOOK_LIB);
    if(!kota::fs::is_file(lib_path)) {
        throw cpptrace::runtime_error(
            std::format("Catter-Proxy Hook library not found at path `{}`", lib_path));
    }

    bool preload_injected = false;
    std::string key_preload_prefix = std::string(catter::config::KEY_PRELOAD) + "=";
    for(auto& env_item: command.env) {
        if(env_item.starts_with(key_preload_prefix)) {
            env_item += catter::config::OS_PATH_SEPARATOR + lib_path;
            preload_injected = true;
            break;
        }
    }

    if(!preload_injected) {
        command.env.push_back(std::format("{}={}", catter::config::KEY_PRELOAD, lib_path));
    }
    command.env.push_back(std::format("{}={}", catter::config::KEY_CATTER_COMMAND_ID, id));
    command.env.push_back(std::format("{}={}",
                                      catter::config::KEY_CATTER_PROXY_PATH,
                                      program_path(catter::config::PROXY_EXE_NAME)));
    command.env.push_back(std::format("{}={}", catter::config::KEY_CATTER_IPC, pipe_name));

    std::string cmd_for_print = "";
    for(auto& arg: command.args) {
        cmd_for_print += " " + arg;
    }
    LOG_INFO("| -> Catter-Proxy Final Executing command: \n    exe = {} \n    args = {}",
             command.executable,
             cmd_for_print);

    return process_options(command);
}

};  // namespace catter
