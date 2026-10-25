#pragma once

#include <kota/async/runtime/task.h>

#include "session/data.h"
#include "support/crossplat.h"
#include "support/encoding.h"

namespace catter::proxy::hook {
/// Run the command with catter proxy hook, its commands reporting to the
/// session's pipe.
kota::task<data::process_result>
    run(data::command command,
        data::ipcid_t id,
        std::string pipe_name,
        std::string proxy_path = util::to_utf8(util::get_executable_path()));
}  // namespace catter::proxy::hook
