#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <stdexcept>
#include <string>
#include <system_error>

#include <eventide/process.h>
#include <vector>

#include "hook.h"
#include "ipc_handler.h"
#include "unix/config.h"

#include "util/crossplat.h"
#include "util/log.h"
#include "util/output.h"
#include "opt-data/catter-proxy/parser.h"
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
                .creation = {.windows_hide = true, .windows_verbatim_arguments = true}
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
}  // namespace catter::proxy

// we do not output in proxy, it must be invoked by main program.
// usage: catter-proxy.exe -p <parent ipc id> --exec <exe path> -- <args...>
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
    auto& ipc_ins = catter::proxy::ipc_handler::instance();

    try {

        auto opt = catter::optdata::catter_proxy::parse_opt(argc, argv);

        if(!opt.argv.has_value()) {
            throw opt.argv.error();
        }
        catter::ipc::data::command cmd = {
            .working_dir = std::filesystem::current_path().string(),
            .executable = opt.executable,
            .args = opt.argv.value(),
            .env = catter::util::get_environment(),
        };

        auto id = ipc_ins.create(std::stoi(opt.parent_id));

        auto received_act = ipc_ins.make_decision(cmd);

        int ret = catter::proxy::run(received_act, id);

        ipc_ins.finish(ret);

        return ret;
    } catch(const std::exception& e) {
        std::string args;
        for(int i = 0; i < argc; ++i) {
            args += std::format("{} ", argv[i]);
        }

        LOG_CRITICAL("Exception in catter-proxy: {}. Args: {}", e.what(), args);
        ipc_ins.report_error(e.what());
        return -1;
    } catch(...) {
        LOG_CRITICAL("Unknown exception in catter-proxy.");
        ipc_ins.report_error("Unknown exception in catter-proxy.");
        return -1;
    }
}
