#pragma once

#include "util/ipc-data.h"
#include <system_error>

namespace catter::proxy::hook {
/// the hook impl should also locate executable if needed
void locate_exe(ipc::data::command& command);

/// Run the command with catter proxy hook
int run(ipc::data::command command, ipc::data::command_id_t id);
}  // namespace catter::proxy::hook
