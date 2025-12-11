#include <stdexcept>

#include "libutil/rpc_data.h"
#include "libutil/crossplat.h"

namespace catter::proxy {
rpc::data::command build_raw_cmd(char* arg_start[], char* arg_end[]) {
    if(arg_start >= arg_end) {
        throw std::invalid_argument("No command provided");
    }
    rpc::data::command cmd;
    cmd.executable = *arg_start;
    for(char** arg_i = arg_start + 1; arg_i < arg_end; ++arg_i) {
        cmd.args.emplace_back(*arg_i);
    }
    cmd.env = catter::util::get_environment();
    return cmd;
}
}  // namespace catter::proxy
