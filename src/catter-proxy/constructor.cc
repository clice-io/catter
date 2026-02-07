#include <filesystem>
#include <stdexcept>

#include "util/ipc-data.h"
#include "util/crossplat.h"

namespace catter::proxy {
ipc::data::command build_raw_cmd(char* arg_start[], char* arg_end[]) {
    if(arg_start >= arg_end) {
        throw std::invalid_argument("No command provided");
    }
    ipc::data::command cmd;
    cmd.working_dir = std::filesystem::current_path().string();
    cmd.executable = *arg_start;
    for(char** arg_i = arg_start + 1; arg_i < arg_end; ++arg_i) {
        cmd.args.emplace_back(*arg_i);
    }
    cmd.env = catter::util::get_environment();
    return cmd;
}
}  // namespace catter::proxy
