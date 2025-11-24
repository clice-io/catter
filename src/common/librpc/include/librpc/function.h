#pragma once
#include "data.h"

namespace catter::rpc::server {

data::command_id_t init(data::command_id_t parent_id);

data::action make_decision(data::command cmd);

void finish(int ret_code);
}  // namespace catter::rpc::server
