#include <cstdlib>
#include <print>
#include <string>
#include <system_error>

#include "constructor.h"
#include "librpc/data.h"
#include "librpc/function.h"
#include "librpc/helper.h"
#include "libutil/crossplat.h"
#include "libhook/interface.h"

namespace catter::proxy {
int run(rpc::data::action act, rpc::data::command_id_t id) {
    using catter::rpc::data::action;
    switch(act.type) {
        case action::WRAP: {
            return std::system(rpc::helper::cmdline_of(act.cmd).c_str());
        }
        case action::INJECT: {
            return catter::proxy::hook::run(act.cmd, id);
        }
        case action::DROP: {
            return 0;
        }
        default: {
            return -1;
        }
    }
}
}  // namespace catter::proxy

int main(int argc, char* argv[], char* envp[]) {
    if(argc < 5) {
        // -p is the parent of this
        std::println("Usage: catter-proxy -p [id] -- <target-exe> [args...]");
        return 0;
    }

    char** arg_end = argv + argc;

    try {

        if(std::string(argv[1]) != "-p") {
            std::println("Expected '-p' as the first argument");
        }

        catter::rpc::data::command_id_t from_id = std::stoi(argv[2]);

        if(std::string(argv[3]) != "--") {
            std::println("Expected '--' as the first argument");
            return 0;
        }

        // 1. read command from args
        auto cmd = catter::proxy::build_raw_cmd(argv + 4, arg_end);

        // 2. locate executable, which means resolve PATH if needed
        catter::util::locate_exe(cmd);
        // 3. remote procedure call, wait server make decision
        // TODO, depend yalantinglib, coro_rpc
        // use interface in librpc/function.h
        // now we just invoke the function, it is wrong in use
        auto id_new = catter::rpc::server::init(from_id);

        auto received_act = catter::rpc::server::make_decision(cmd);

        // 4. run command
        int ret = catter::proxy::run(received_act, id_new);

        // 5. report finish
        catter::rpc::server::finish(ret);

        // 5. return exit code
        return ret;
    } catch(const std::exception& e) {
        std::println("catter-proxy encountered an error: {}", e.what());
        return -1;
    }
}
