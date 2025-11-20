#include <print>
#include <span>
#include <string>

#include "command.h"
#include "hook.h"

namespace catter {
struct Action {
    enum { WRAP, INJECT, DROP } type;

    Command cmd;
};
}  // namespace catter

namespace catter::proxy {
int run(Action act) {
    switch(act.type) {
        case Action::WRAP: return catter::spawn(act.cmd);
        case Action::INJECT: return catter::hook::run(act.cmd);
        case Action::DROP: return 0;
        default: return -1;
    }
}
}  // namespace catter::proxy

namespace catter::proxy::rpc {

Action decision(Command cmd) {
    // For demonstration purposes, always return INJECT action
    return Action{Action::INJECT, cmd};
}

void finish(int ret_code) {
    // For demonstration purposes, just print the return code
    std::println("Process finished with code {}", ret_code);
}

}  // namespace catter::proxy::rpc

int main(int argc, char* argv[], char* envp[]) {

    if(argc < 3) {
        std::println("Usage: catter-proxy -- <target-exe> [args...]");
        return 0;
    }

    if(std::string(argv[1]) != "--") {
        std::println("Expected '--' as the first argument");
        return 0;
    }

    try {
        // 1. read command from args
        catter::Command cmd = catter::Command::create({argv + 2, argv + argc}, envp);

        // 2. remote procedure call, wait server make decision
        catter::Action act = catter::proxy::rpc::decision(cmd);

        // 3. run action
        int ret = catter::proxy::run(act);

        // 4. report finish status to server
        catter::proxy::rpc::finish(ret);

        // 5. return exit code
        return ret;
    } catch(const std::exception& e) {
        std::println("catter-proxy encountered an error: {}", e.what());
        return -1;
    }
}
