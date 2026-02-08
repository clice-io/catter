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
#include "linux-mac/config.h"


namespace catter::proxy::hook {

void locate_exe(ipc::data::command& command) {
    std::string result;
    std::array<char, 128> buffer;

    // we use `command -v` instead of `which`, because formmer is in POSIX standard.
    std::string find_cmd = "command -v " + command.executable;
    auto fp = popen(find_cmd.c_str(), "r");
    if(!fp) {
        throw std::runtime_error("popen failed when locating executable");
    }
    if(fgets(buffer.data(), buffer.size(), fp) != nullptr) {
        result = buffer.data();
        // remove trailing newline
        if(!result.empty() && result.back() == '\n') {
            result.pop_back();
        }
    }
    auto ret = pclose(fp);
    if(ret == -1 || WEXITSTATUS(ret) != 0) {
        throw std::runtime_error("command -v failed to locate executable");
    }
    if(result.empty()) {
        throw std::runtime_error("executable not found");
    }
    command.executable = result;
}

std::filesystem::path get_hook_path() {
    auto exe_path = util::get_executable_path();
    return std::filesystem::path(exe_path).parent_path() /
           catter::config::hook::RELATIVE_PATH_OF_HOOK_LIB;
}

int run(ipc::data::command command, ipc::data::command_id_t id) {
    const auto lib_path = get_hook_path();
    LOG_INFO("new command id is: {}", id);
    // check hook_lib exists
    if(!std::filesystem::exists(lib_path)) {
        throw std::runtime_error(
            std::format("Catter-Proxy Hook library not found at path: {}", lib_path.string()));
    }

    command.env.push_back(std::format("{}={}",
                                      //   "/usr/lib/gcc/x86_64-linux-gnu/14/libasan.so",
                                      catter::config::hook::KEY_PRELOAD,
                                      lib_path.string()));
    command.env.push_back(std::format("{}={}", catter::config::hook::KEY_CATTER_COMMAND_ID, id));
    command.env.push_back(std::format("{}={}",
                                      catter::config::hook::KEY_CATTER_PROXY_PATH,
                                      util::get_executable_path().string()));

    std::string cmd_for_print = "";
    cmd_for_print += command.executable;
    for(auto& arg: command.args) {
        cmd_for_print += " " + arg;
    }
    LOG_INFO("| -> Catter-Proxy Final Executing command: {}", cmd_for_print);
    
    eventide::process::options opts{
        .file = command.executable,
        .args = command.args,
        .env = command.env,
        .cwd = command.working_dir,
    };
    return catter::wait(catter::spawn(opts));
};

};  // namespace catter::proxy::hook
