
#include "librpc/function.h"
#include "librpc/data.h"
#include <cstdlib>
#include <print>

// TODO
namespace catter::rpc::server {

data::decision_info make_decision(data::command_id_t parent_id, data::command cmd) {
    // placeholder
    return data::decision_info{
        data::action{data::action::INJECT, cmd},
        parent_id + 1
    };
}

void report_error(data::command_id_t parent_id, std::string error_msg) {
    std::println("Error reported for command id {}: {}", parent_id, error_msg);  // placeholder
    std::exit(-1);
};

void finish(data::command_id_t parent_id, int ret_code) {
    return;
}
}  // namespace catter::rpc::server
