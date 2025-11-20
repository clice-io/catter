#include "hook/interface.h"
#include "./libhook/config.h"
#include "config.h"
#include "common/crossplat.h"
#include <array>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits.h>
#include <print>
#include <system_error>
#include <unistd.h>
#include <dirent.h>
#include <vector>

namespace catter::hook {

std::filesystem::path get_hook_path(std::error_code& ec) {
    auto exe_path = get_executable_path(ec);
    if(ec) {
        return {};
    }
    return std::filesystem::path(exe_path).parent_path() / config::RELATIVE_PATH_OF_HOOK_LIB;
}

std::filesystem::path get_save_dir(std::error_code& ec) {
    const char* home = getenv("HOME");
    if(home == nullptr) {
        ec = std::make_error_code(std::errc::no_such_file_or_directory);
        return {};
    }
    std::string this_dir = std::format("{}-{}", getpid(), get_thread_id());
    return std::filesystem::path(home) / config::HOME_RELATIVE_PATH_OF_LOG / this_dir;
}

int run(std::span<const char* const> command, std::error_code& ec) {
    if(command.empty()) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return 0;
    }
    const auto lib_path = get_hook_path(ec);
    const auto save_dir = get_save_dir(ec);
    if(ec) {
        return 0;
    }
    std::println("Using hook library at path: {}", lib_path.string());
    std::println("Saving captured logs to directory: {}", save_dir.string());
    // check hook_lib exists
    if(!std::filesystem::exists(lib_path, ec)) {
        ec = std::make_error_code(std::errc::no_such_file_or_directory);
        return 0;
    }
    // delete and recreate save_dir
    if(std::filesystem::exists(save_dir, ec)) {
        if(ec) {
            return 0;
        }
        std::filesystem::remove_all(save_dir, ec);
        if(ec) {
            return 0;
        }
    }

    std::filesystem::create_directories(save_dir, ec);
    if(ec) {
        return 0;
    }
    std::string joined_command = "";
#ifdef CATTER_MAC
#ifdef DEBUG
    joined_command += "DYLD_PRINT_INTERPOSING=1 ";
#endif  // DEBUG
#endif  // CATTER_MAC

    joined_command += std::format("{}={} {}={} {}={} ",
                                  config::KEY_PRELOAD,
                                  lib_path.string(),
                                  config::KEY_CMD_LOG_FILE,
                                  save_dir.string(),
                                  config::KEY_CATTER_PRELOAD_PATH,
                                  lib_path.string());
    for(const auto& arg: command) {
        // consider if the user add LD_PRELOAD in command
        if(auto arg_sv = std::string_view{arg}; arg_sv.starts_with(config::KEY_PRELOAD)) {
            arg_sv.remove_prefix(std::char_traits<char>::length(config::KEY_PRELOAD));
            if(auto pos = arg_sv.find('='); pos != std::string_view::npos) {
                std::string existing_paths = std::string(arg_sv.substr(pos + 1));
                // insert firstly
                joined_command += std::format("{}={}{}{}:{} ",
                                              config::KEY_PRELOAD,
                                              lib_path.string(),
                                              config::OS_PATH_SEPARATOR,
                                              existing_paths,
                                              config::OS_PATH_SEPARATOR);
                continue;
            }
        }
        joined_command += std::format("{} ", arg);
    }
    std::println("final cmd is {}", joined_command);
    // sh -c the command with hook injected
    // run
    const int ret = system(joined_command.c_str());
    if(ret != 0) {
        ec = std::make_error_code(std::errc::operation_canceled);
    }
    return 0;
};

std::expected<std::vector<std::string>, std::string> collect_all() {
    auto ec = std::error_code{};
    auto save_dir = get_save_dir(ec);
    if(ec) {
        return std::unexpected(std::format("Failed to get save directory: {}", ec.message()));
    }
    std::vector<std::string> result;
    if(!std::filesystem::exists(save_dir, ec)) {
        return std::unexpected(std::format("Save directory does not exist: {}", save_dir.string()));
    }
    // tranverse this dir to find all files
    for(const auto& entry: std::filesystem::directory_iterator(save_dir, ec)) {
        if(ec) {
            return std::unexpected(std::format("Failed to access directory entry: {}: {}",
                                               entry.path().string(),
                                               ec.message()));
        }

        if(entry.is_regular_file()) {
            std::println("Collecting captured log from file: {}", entry.path().string());
            // read text
            std::ifstream ifs(entry.path());
            if(!ifs.is_open()) {
                return std::unexpected(
                    std::format("Failed to open commands file: {}", entry.path().string()));
            }
            std::string line;
            while(std::getline(ifs, line)) {
                if(line.starts_with(config::ERROR_PREFFIX)) {
                    return std::unexpected(std::format(
                        "Error captured: {}",
                        line.substr(std::char_traits<char>::length(config::ERROR_PREFFIX))));
                } else {
                    result.push_back(line);
                }
            }
        }
    }
    // remove cache
    std::filesystem::remove_all(save_dir, ec);
    if(ec) {
        return std::unexpected(std::format("Failed to remove save directory: {}", ec.message()));
    }
    return result;
};

};  // namespace catter::hook
