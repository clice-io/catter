#pragma once

#include "util/ipc-data.h"
#include <system_error>

namespace catter::proxy::hook {
/// Run the command with catter proxy hook
int run(ipc::data::command command, ipc::data::command_id_t id);
}  // namespace catter::proxy::hook
