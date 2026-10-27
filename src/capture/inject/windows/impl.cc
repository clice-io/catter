module;

#include "modules/prelude.h"

#include <windows.h>

#include "support/messages.h"

module catter;

import :capture.inject.hook;
import :capture.inject.windows.inject;
import :session.process;

namespace catter {

kota::process::options hook_options(data::command command,
                                    data::ipcid_t id,
                                    std::string_view pipe_name) {
    auto opts = process_options(command);
    opts.windows_before_start = [executable = std::move(command.executable),
                                 id,
                                 pipe = std::string(pipe_name)](void* process, void*) {
        inject_hook(static_cast<HANDLE>(process), executable, id, pipe);
        return std::error_code();
    };
    return opts;
}

}  // namespace catter
