#pragma once
#include "librpc/data.h"

namespace catter::rpc::server {

data::decision_info make_decision(data::command_id_t parent_id, data::command cmd);

/**
 * @brief Report an error to the server
 * Every error in proxy and hook will finally report to main program.
 * If we cannot get parent_id, parent_id = -1
 */
void report_error(data::command_id_t parent_id, std::string error_msg) noexcept;

void finish(int ret_code);

}  // namespace catter::rpc::server
