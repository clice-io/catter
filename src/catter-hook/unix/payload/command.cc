#include "command.h"

#include <format>

#include "session.h"

namespace {
void push_proxy_args(std::vector<std::string>& argv,
                     const catter::Session& sess,
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

std::vector<char*> Command::c_argv() {
    std::vector<char*> res;
    res.reserve(argv.size() + 1);
    for(auto& arg: argv) {
        res.push_back(arg.data());
    }
    res.push_back(nullptr);
    return res;
}

Command build_proxy_command(const Session& session, const fs::path& path, ArgvRef argv) {
    Command cmd;
    cmd.path = session.proxy_path;
    push_proxy_args(cmd.argv, session, path.string());
    for(const auto arg: argv) {
        cmd.argv.emplace_back(arg);
    }
    return cmd;
}

Command build_error_command(const Session& session,
                            std::string_view message,
                            const fs::path& path,
                            ArgvRef argv) {
    Command cmd;
    cmd.path = session.proxy_path;
    push_proxy_args(cmd.argv, session, path.string(), true);
    std::string res_msg = std::format("Catter Proxy Error: {}\n", message);
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
