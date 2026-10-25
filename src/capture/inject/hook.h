#pragma once

#include <kota/async/runtime/task.h>

#include "session/data.h"
#include "support/crossplat.h"
#include "support/encoding.h"

namespace catter {
/// Run the command with catter proxy hook, its commands reporting to the
/// session's pipe.
kota::task<data::process_result>
    run_with_hook(data::command command,
                  data::ipcid_t id,
                  std::string pipe_name,
                  std::string proxy_path = to_utf8(get_executable_path()));
}  // namespace catter
