#pragma once

#include "util/crossplat.h"
#include "util/data.h"

namespace catter::proxy::hook {
/// Run the command with catter proxy hook
kota::task<data::process_result> run(data::command command,
                                     data::ipcid_t id,
                                     std::string proxy_path = util::get_executable_path().string());
}  // namespace catter::proxy::hook
