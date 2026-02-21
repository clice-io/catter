#pragma once

#include "util/ipc-data.h"
#include "util/crossplat.h"

namespace catter::proxy::hook {
/// Run the command with catter proxy hook
int run(ipc::data::command command,
        ipc::data::command_id_t id,
        std::string proxy_path = util::get_executable_path().string());
}  // namespace catter::proxy::hook
