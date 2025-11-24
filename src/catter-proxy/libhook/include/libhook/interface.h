#pragma once

#include "librpc/data.h"

namespace catter::proxy::hook {
int run(rpc::data::command command, rpc::data::command_id_t id);
}  // namespace catter::proxy::hook
