#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <string>
#include <system_error>

#include <eventide/process.h>

#include "hook.h"
#include "ipc_handler.h"
#include "linux-mac/config.h"

#include "util/crossplat.h"
#include "util/log.h"
#include "util/output.h"
#include "config/catter-proxy.h"

namespace catter::proxy {
int run(ipc::data::action act, ipc::data::command_id_t id) {
    using catter::ipc::data::action;
    switch(act.type) {
        case action::WRAP: {
            eventide::process::options opts{
                .file = act.cmd.executable,
                .args = act.cmd.args,
                .cwd = act.cmd.working_dir,
                .creation =
                    {
                               .windows_hide = true,
                               .windows_verbatim_arguments = true,
                               },
            };
            return wait(spawn(opts));
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

ipc::data::command build_raw_cmd(char* arg_start[], char* arg_end[]) {
    if(arg_start >= arg_end) {
        throw std::invalid_argument("No command provided");
    }
    ipc::data::command cmd;
    cmd.working_dir = std::filesystem::current_path().string();
    cmd.executable = *arg_start;
    for(char** arg_i = arg_start; arg_i < arg_end; ++arg_i) {
        cmd.args.emplace_back(*arg_i);
    }
    cmd.env = catter::util::get_environment();
    return cmd;
}

}  // namespace catter::proxy

// we do not output in proxy, it must be invoked by main program.
int main(int argc, char* argv[], char* envp[]) {
    try {
        catter::log::init_logger("catter-proxy.log",
                                 catter::util::get_catter_data_path() /
                                     catter::config::proxy::LOG_PATH_REL,
                                 false);
    } catch(const std::exception& e) {
        // cannot init logger
        catter::log::mute_logger();
    }

    // single instance of ipc handler
    auto& ipc_ins = catter::proxy::ipc_handler::instance();

    if(argc < 4) {
        // -p is the parent of this
        LOG_CRITICAL("Expected at least 4 arguments, got {}", argc);
        ipc_ins.report_error("Insufficient arguments in catter-proxy");
        return -1;
    }

    char** arg_end = argv + argc;

    try {

        if(std::string(argv[1]) != "-p") {
            LOG_CRITICAL("Expected '-p' as the first argument");
            ipc_ins.report_error("Invalid arguments in catter-proxy");
            return -1;
        }

        catter::ipc::data::command_id_t parent_id = std::stoi(argv[2]);

        auto id = ipc_ins.create(parent_id);

        if(std::string(argv[3]) != "--") {
            if(argv[3] != nullptr) {
                // a msg from hook
                ipc_ins.report_error(argv[3]);
                return -1;
            }
            LOG_CRITICAL("Expected '--' as the third argument");
            ipc_ins.report_error("Invalid arguments in catter-proxy");
            return -1;
        }

        // 1. read command from args
        auto cmd = catter::proxy::build_raw_cmd(argv + 4, arg_end);

        // 2. locate executable, which means resolve PATH if needed
        catter::proxy::hook::locate_exe(cmd);

        // 3. remote procedure call, wait server make decision
        auto received_act = ipc_ins.make_decision(cmd);
        // received cmd maybe not a path, either, so we need locate again
        catter::proxy::hook::locate_exe(received_act.cmd);

        // 4. run command
        int ret = catter::proxy::run(received_act, id);

        // 5. report finish
        ipc_ins.finish(ret);

        // 5. return exit code
        return ret;
    } catch(const std::exception& e) {
        LOG_CRITICAL("Exception in catter-proxy: {}", e.what());
        ipc_ins.report_error(e.what());
        return -1;
    } catch(...) {
        LOG_CRITICAL("Unknown exception in catter-proxy.");
        ipc_ins.report_error("Unknown exception in catter-proxy.");
        return -1;
    }
}
