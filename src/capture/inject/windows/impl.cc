module;

#include "modules/prelude.h"

#include <windows.h>

#include "support/config/catter-proxy.h"
#include "support/config/hook_windows.h"
#include "support/log.h"
#include "support/paths.h"

module catter;

import :capture.inject.hook;
import :capture.inject.windows.inject;
import :session.data;
import :session.process;

namespace catter {

kota::process::options hook_options(data::command command,
                                    data::ipcid_t id,
                                    std::string_view pipe_name) {
    LOG_INFO("new command id is: {}", id);
    auto opts = process_options(command);
    opts.env_changes = {
        {.name = config::ENV_VAR_IPC_ID,     .value = std::to_string(id)                  },
        {.name = config::ENV_VAR_PROXY_PATH, .value = program_path(config::PROXY_EXE_NAME)},
        {.name = config::ENV_VAR_IPC_PIPE,   .value = std::string(pipe_name)              },
    };
    opts.windows_before_start = [executable = std::move(command.executable)](void* process, void*) {
        inject_hook(static_cast<HANDLE>(process), executable);
        return std::error_code();
    };
    return opts;
}

}  // namespace catter
