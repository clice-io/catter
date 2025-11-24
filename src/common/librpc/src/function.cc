
#include "librpc/function.h"

namespace catter::rpc::server {


data::command_id_t init(data::command_id_t parent_id){
    return 114514; // placeholder
}

data::action make_decision(data::command cmd){
    return {
        .type = data::action::INJECT,
        .cmd = cmd
    };
}

void finish(int ret_code){
    return;
}
}  // namespace catter::rpc::server
