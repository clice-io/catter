#include "librpc/data.h"
#include "libutil/crossplat.h"
#include <expected>
#include <chrono>

// namespace catter {
// namespace detail {

// Command Command::create(std::span<cstr_view> args, cstr_view envp[]) {

//     if(args.size() == 0) {
//         throw std::invalid_argument("No arguments provided");
//     }

//     // TODO: resolve exe path
//     std::filesystem::path exe_path = args[0];

//     std::vector<std::string> arguments = {args.begin() + 1, args.end()};
//     std::map<std::string, std::string> environment;
//     for(cstr_view* it = envp; *it != nullptr; ++it) {
//         std::string_view env_entry(*it);
//         size_t pos = env_entry.find('=');
//         if(pos != std::string_view::npos) {
//             environment.emplace(std::string(env_entry.substr(0, pos)),
//                                 std::string(env_entry.substr(pos + 1)));
//         } else {
//             environment.emplace(std::string(env_entry), "");
//         }
//     }
//     return Command{std::move(exe_path), std::move(arguments), std::move(environment)};
// }

// int spawn(Command cmd) {
//     // For demonstration purposes, just use std::system to run the command
//     return std::system(cmd.cmdline().c_str());
// }
// }  // namespace catter

namespace catter::proxy {
std::expected<rpc::data::Command, const char*> buildRawCommand(char** arg_start, char** arg_end) {
    if(arg_start >= arg_end) {
        return std::unexpected("No command provided");
    }
    rpc::data::Command cmd;
    cmd.executable = *arg_start;
    for(char** arg_i = arg_start + 1; arg_i < arg_end; ++arg_i) {
        cmd.args.emplace_back(*arg_i);
    }
    cmd.env = catter::util::get_environment();
    return cmd;
}

std::expected<rpc::data::DataToCatter, std::string> buildData(rpc::data::Command cmd,
                                                              rpc::data::command_id_t from_id) {
    rpc::data::DataToCatter toCatter;
    toCatter.cmd = std::move(cmd);
    toCatter.from_id = from_id;
    toCatter.timestamp =
        static_cast<rpc::data::timestamp_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                std::chrono::system_clock::now().time_since_epoch())
                                                .count());
    return toCatter;
}
}  // namespace catter::proxy
