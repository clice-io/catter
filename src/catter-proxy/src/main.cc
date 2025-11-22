#include <cstdlib>
#include <print>
#include <string>
#include <system_error>

#include "constructor.h"
#include "librpc/data.h"
#include "librpc/function.h"
#include "interface/hook.h"
#include "librpc/helper.h"

namespace catter {
// struct Action {
//     enum { WRAP, INJECT, DROP } type;

//     Command cmd;
// };
}  // namespace catter

namespace catter::proxy {
int runCmd(rpc::data::DataToProxy info, std::error_code& ec) {
    using catter::rpc::data::Action;
    switch(info.action) {
        case Action::NEGLECT: return std::system(rpc::helper::cmdlineOf(info.command).c_str());
        case Action::INJECT: return catter::proxy::impl::run(info.command, info.new_cmd_id, ec);
        case Action::DROP: return 0;
        default: return -1;
    }
}
}  // namespace catter::proxy

int main(int argc, char* argv[], char* envp[]) {
    if(argc < 5) {
        // -p is the parent of this
        std::println("Usage: catter-proxy -p [id] <target-exe> [args...]");
        return 0;
    }

    char** cur_argv = argv;
    char** arg_end = argv + argc;

    if(std::string(*cur_argv++) != "-p") {
        std::println("Expected '-p' as the first argument");
    }

    catter::rpc::data::command_id_t from_id = std::stoi(*cur_argv++);

    if(std::string(*cur_argv++) != "--") {
        std::println("Expected '--' as the first argument");
        return 0;
    }

    try {
        // 1. read command from args
        auto rawCmdRes = catter::proxy::buildRawCommand(cur_argv, arg_end);
        std::error_code ec;
        if(!rawCmdRes.has_value()) {
            std::println("Error building command: {}", rawCmdRes.error());
            return -1;
        }
        auto cmd = rawCmdRes.value();
        // 2. locate executable, which means resolve PATH if needed
        catter::proxy::impl::locateExecutable(cmd, ec);
        if(ec) {
            std::println("Error locating executable: {}", ec.message());
            return -1;
        }

        // 3. remote procedure call, wait server make decision
        // TODO, depend yalantinglib, coro_rpc
        // use interface in librpc/function.h
        // now we just invoke the function, it is wrong in use
        auto dataRes = catter::proxy::buildData(cmd, from_id);
        if(!dataRes.has_value()) {
            std::println("Error building data to catter: {}", dataRes.error());
            return -1;
        }
        auto data = dataRes.value();
        auto received = catter::rpc::server::makeDecision(data);
        auto cur_id = received.new_cmd_id;

        // 4. run command
        int ret = catter::proxy::runCmd(received, ec);

        // 5. report finish
        if(received.action == catter::rpc::data::Action::INJECT) {
            catter::rpc::server::reportFinish(cur_id, ret);
        }

        // 5. return exit code
        return ret;
    } catch(const std::exception& e) {
        std::println("catter-proxy encountered an error: {}", e.what());
        return -1;
    }
}
