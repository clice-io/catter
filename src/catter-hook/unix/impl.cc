#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <dirent.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cpptrace/exceptions.hpp>
#if defined(CATTER_LINUX)
#include <link.h>
#endif

#include "hook.h"
#include "unix/config.h"
#include "util/crossplat.h"
#include "util/data.h"
#include "util/kotatsu.h"
#include "util/log.h"

#if defined(__SANITIZE_ADDRESS__)
#define CATTER_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define CATTER_ASAN 1
#endif
#endif

namespace catter::proxy::hook {

#if defined(CATTER_LINUX) && defined(CATTER_ASAN)
/// The sanitizer runtime this program loaded, which a sanitized hook library
/// needs loaded before it in every program of the build.
static std::string sanitizer_runtime() {
    std::string runtime;
    dl_iterate_phdr(
        [](dl_phdr_info* info, size_t, void* data) {
            auto name = std::filesystem::path(info->dlpi_name).filename().string();
            if(name.starts_with("libasan") || name.starts_with("libclang_rt.asan")) {
                *static_cast<std::string*>(data) = info->dlpi_name;
                return 1;
            }
            return 0;
        },
        &runtime);
    if(runtime.empty()) {
        throw cpptrace::runtime_error("the sanitized hook library needs a shared ASan runtime");
    }
    return runtime;
}

/// A build's programs are not sanitized: the runtime goes first in their
/// preload, and leak detection, which would report their own leaks at exit,
/// stays off unless the build set it.
static void add_sanitizer_runtime(std::vector<std::string>& env,
                                  std::string_view key_preload_prefix) {
    auto preload = std::ranges::find_if(env, [&](const std::string& item) {
        return item.starts_with(key_preload_prefix);
    });
    auto first = sanitizer_runtime() + catter::config::OS_PATH_SEPARATOR;
    if(!preload->substr(key_preload_prefix.size()).starts_with(first)) {
        preload->insert(key_preload_prefix.size(), first);
    }

    auto options = std::ranges::find_if(env, [](const std::string& item) {
        return item.starts_with("ASAN_OPTIONS=");
    });
    if(options == env.end()) {
        env.push_back("ASAN_OPTIONS=detect_leaks=0");
    } else if(!options->contains("detect_leaks")) {
        *options += ":detect_leaks=0";
    }
}
#endif

kota::task<data::process_result>
    run(data::command command, data::ipcid_t id, std::string pipe_name, std::string proxy_path) {
    LOG_INFO("new command id is: {}", id);

    const auto lib_path =
        util::get_catter_root_path() / catter::config::hook::RELATIVE_PATH_OF_HOOK_LIB;

    // check hook_lib exists
    if(!std::filesystem::exists(lib_path)) {
        throw cpptrace::runtime_error(
            std::format("Catter-Proxy Hook library not found at path `{}`", lib_path.string()));
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
#if defined(CATTER_LINUX) && defined(CATTER_ASAN)
    add_sanitizer_runtime(command.env, key_preload_prefix);
#endif
    command.env.push_back(std::format("{}={}", catter::config::hook::KEY_CATTER_COMMAND_ID, id));
    command.env.push_back(
        std::format("{}={}", catter::config::hook::KEY_CATTER_PROXY_PATH, proxy_path));
    command.env.push_back(std::format("{}={}", catter::config::hook::KEY_CATTER_IPC, pipe_name));

    std::string cmd_for_print = "";
    for(auto& arg: command.args) {
        cmd_for_print += " " + arg;
    }
    LOG_INFO("| -> Catter-Proxy Final Executing command: \n    exe = {} \n    args = {}",
             command.executable,
             cmd_for_print);

    return catter::run_process({
        .file = std::move(command.executable),
        .args = std::move(command.args),
        .env = std::move(command.env),
        .cwd = std::move(command.cwd),
    });
};

};  // namespace catter::proxy::hook
