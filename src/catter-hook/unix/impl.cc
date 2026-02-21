#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>

#include <limits.h>
#include <spawn.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>

#include "hook.h"
#include "util/ipc-data.h"
#include "util/crossplat.h"
#include "util/log.h"
#include "util/eventide.h"
#include "unix/config.h"

namespace catter::proxy::hook {

int run(ipc::data::command command, ipc::data::command_id_t id, std::string proxy_path) {
    LOG_INFO("new command id is: {}", id);

    const auto lib_path =
        util::get_catter_root_path() / catter::config::hook::RELATIVE_PATH_OF_HOOK_LIB;

    // check hook_lib exists
    if(!std::filesystem::exists(lib_path)) {
        throw std::runtime_error(
            std::format("Catter-Proxy Hook library not found at path: {}", lib_path.string()));
    }

    bool preload_injected = false;
    std::string key_preload_prefix = std::string(catter::config::hook::KEY_PRELOAD) + "=";
    for(auto& env_item: command.env) {
        if(env_item.starts_with(key_preload_prefix)) {
            env_item += catter::config::OS_PATH_SEPARATOR + lib_path.string();
            preload_injected = true;
            break;
        }
    }

    if(!preload_injected) {
        command.env.push_back(
            std::format("{}={}", catter::config::hook::KEY_PRELOAD, lib_path.string()));
    }
    command.env.push_back(std::format("{}={}", catter::config::hook::KEY_CATTER_COMMAND_ID, id));
    command.env.push_back(
        std::format("{}={}", catter::config::hook::KEY_CATTER_PROXY_PATH, proxy_path));

    std::string cmd_for_print = "";
    for(auto& arg: command.args) {
        cmd_for_print += " " + arg;
    }
    LOG_INFO("| -> Catter-Proxy Final Executing command: \n    exe = {} \n    args = {}",
             command.executable,
             cmd_for_print);

    eventide::process::options opts{
        .file = command.executable,
        .args = command.args,
        .env = command.env,
        .cwd = command.working_dir,
    };
    return catter::wait(catter::spawn(opts));
};

};  // namespace catter::proxy::hook
