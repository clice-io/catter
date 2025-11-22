#pragma once
#include "data.h"

namespace catter::rpc::server {
data::DataToProxy makeDecision(const data::DataToCatter& data);
void reportFinish(data::command_id_t id, int ret_code);
}  // namespace catter::rpc::server
