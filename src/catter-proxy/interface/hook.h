#pragma once
#include "librpc/data.h"
#include <system_error>

namespace catter::proxy::impl {
int run(rpc::data::Command command, rpc::data::command_id_t id, std::error_code& ec);
void locateExecutable(rpc::data::Command& command, std::error_code& ec);
}  // namespace catter::proxy::impl
