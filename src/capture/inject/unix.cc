module;

#include "support/prelude.h"

#if defined(CATTER_MAC)
#include <mach-o/utils.h>
#endif

#include "support/hook.h"
#include "support/log.h"
#include "support/messages.h"
#include "support/paths.h"

module catter;

import :capture.inject;
import :session.process;

namespace catter {

#if defined(CATTER_MAC)
/// The architecture `executable` runs as, as dyld names it; a script runs as
/// its interpreter, and a file of no executable format as /bin/sh, which the
/// program is started with then. Empty for what this Mac cannot run.
static std::string_view arch_of(std::string_view executable) {
    std::array<char, 512> head;
    std::size_t size = 0;
    if(auto file = fs::File::open(executable)) {
        size = file->read(head).value_or(0);
    }
    std::string_view line(head.data(), size);
    std::string program(executable);
    if(line.starts_with("#!")) {
        line.remove_prefix(2);
        line.remove_prefix(std::min(line.find_first_not_of(" \t"), line.size()));
        program = line.substr(0, line.find_first_of(" \t\n"));
        if(program.empty()) {
            program = "/bin/sh";
        }
    }

    __block std::string_view arch;
    auto slice_arch = ^(const mach_header* slice, uint64_t, size_t) {
        arch = macho_arch_name_for_mach_header(slice);
    };
    // An empty file is EINVAL.
    if(auto error = macho_best_slice(program.c_str(), slice_arch);
       error == EFTYPE || error == EINVAL) {
        macho_best_slice("/bin/sh", slice_arch);
    }
    return arch;
}
#endif

async::process::options hook_options(data::command command,
                                     data::ipcid_t id,
                                     std::string_view pipe_name) {
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

    const auto library = program_path(hook::LIBRARY);
    const auto preload = std::string(hook::PRELOAD) + "=";
    if(auto entry =
           ranges::find_if(command.env,
                           [&](std::string_view entry) { return entry.starts_with(preload); });
       entry != command.env.end()) {
        *entry += hook::PRELOAD_SEPARATORS[0] + library;
    } else {
        command.env.push_back(preload + library);
    }
    command.env.push_back(std::format("{}={}", env::COMMAND_ID, id));
    command.env.push_back(std::format("{}={}", env::PROXY, program_path(PROXY)));
    command.env.push_back(std::format("{}={}", env::IPC, pipe_name));
    LOG_INFO("Command {} runs `{}` with the hook", id, command.executable);
    return process_options(command);
}

}  // namespace catter
