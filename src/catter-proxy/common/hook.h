#pragma once

#include "uv/rpc_data.h"
#include <system_error>

namespace catter::proxy::hook {
/// the hook impl should also locate executable if needed
void locate_exe(rpc::data::command& command);

/// Run the command with catter proxy hook
int run(rpc::data::command command, rpc::data::command_id_t id);
}  // namespace catter::proxy::hook
