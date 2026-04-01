#pragma once

#include "util/data.h"
#include "util/crossplat.h"

namespace catter::proxy::hook {
/// Run the command with catter proxy hook
data::process_result run(data::command command,
                         data::ipcid_t id,
                         std::string proxy_path = util::get_executable_path().string());
}  // namespace catter::proxy::hook
