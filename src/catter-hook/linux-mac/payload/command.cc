#include "command.h"
#include "session.h"
#include <format>

namespace {
void push_proxy_args(catter::CmdBuilder::command::ArgvTy& argv,
                     catter::Session& sess,
                     std::string_view exec_path,
                     bool error = false) {
    argv.emplace_back(sess.proxy_path);
    argv.emplace_back("-p");
    argv.emplace_back(sess.self_id);
    argv.emplace_back("--exec");
    argv.emplace_back(exec_path);
    if(!error) {
        argv.emplace_back("--");
    }
}
};  // namespace

namespace fs = std::filesystem;

namespace catter {

CmdBuilder::command CmdBuilder::proxy_cmd(const fs::path& path, ArgvRef argv) {
    command cmd;
    cmd.path = session_.proxy_path;
    push_proxy_args(cmd.argv, session_, path.string());
    for(const auto arg: argv) {
        cmd.argv.emplace_back(arg);
    }
    return cmd;
}

CmdBuilder::command CmdBuilder::error_cmd(const char* msg, const fs::path& path, ArgvRef argv) {
    command cmd;
    cmd.path = session_.proxy_path;
    push_proxy_args(cmd.argv, session_, path.string(), true);
    std::string res_msg = std::format("Catter Proxy Error: {}\n", msg);
    if(!argv.empty()) {
        res_msg.append(std::format("in command: "));
        for(const auto arg: argv) {
            res_msg += arg;
            res_msg += ' ';
        }
    }
    cmd.argv.emplace_back(res_msg);
    return cmd;
}

}  // namespace catter
