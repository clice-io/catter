#include <format>
#include <string>
#include <system_error>
#include <filesystem>
#include <vector>
#include <cassert>
#include <cstdint>
#include <string>
#include <string_view>
#include <stdexcept>
#include <string.h>

#include <eventide/reflection/enum.h>

#include <windows.h>
#include <libloaderapi.h>
#include <minwindef.h>
#include <Psapi.h>

#include "util/log.h"
#include "util/data.h"
#include "util/crossplat.h"

#include "win/env.h"
#include "win/win32.h"
#include "win/inject.h"

namespace catter::proxy::hook {

namespace {
std::string cmdline_of(const catter::data::command& cmd) noexcept {
    std::string full_cmd;
    for(const auto& arg: cmd.args) {
        full_cmd += win::quote_win32_arg(arg) + " ";
    }
    return full_cmd;
}

bool env_key_equals(std::string_view env_entry, std::string_view key) noexcept {
    const auto separator = env_entry.find('=');
    return separator == key.size() && _strnicmp(env_entry.data(), key.data(), key.size()) == 0;
}

void upsert_environment_variable(std::vector<std::string>& env,
                                 std::string_view key,
                                 std::string value) {
    std::string entry = std::format("{}={}", key, value);
    for(auto& existing: env) {
        if(env_key_equals(existing, key)) {
            existing = std::move(entry);
            return;
        }
    }

    env.push_back(std::move(entry));
}

std::vector<char> build_environment_block(std::vector<std::string> env) {
    size_t env_block_size = 1;
    for(const auto& entry: env) {
        env_block_size += entry.size() + 1;
    }

    std::vector<char> env_block;
    env_block.reserve(env_block_size);

    for(const auto& entry: env) {
        std::span<const char> span(entry.c_str(), entry.size() + 1);
        env_block.append_range(span);
    }

    env_block.push_back('\0');
    return env_block;
}
}  // namespace

int64_t run(data::command cmd, data::ipcid_t id, std::string proxy_path) {

    LOG_INFO("new command id is: {}", id);

    auto env = std::move(cmd.env);
    upsert_environment_variable(env, win::ENV_VAR_IPC_ID<char>, std::to_string(id));
    upsert_environment_variable(env, win::ENV_VAR_PROXY_PATH<char>, proxy_path);

    auto env_block = build_environment_block(std::move(env));  // Double null termination

    PROCESS_INFORMATION pi{};
    STARTUPINFOA si{.cb = sizeof(STARTUPINFOA)};
    auto pi_guard = win::make_guard([&] noexcept -> void {
        if(pi.hProcess) {
            DWORD dwExitCode;
            if(GetExitCodeProcess(pi.hProcess, &dwExitCode)) {
                if(dwExitCode == STILL_ACTIVE) {
                    LOG_ERROR("Process is still active during cleanup, terminating it forcefully");
                    TerminateProcess(pi.hProcess, -1);
                }
            } else {
                LOG_ERROR("Failed to get exit code of process during cleanup: {}",
                          std::system_error(GetLastError(), std::system_category()).what());
            }
            CloseHandle(pi.hProcess);
        }
        if(pi.hThread) {
            CloseHandle(pi.hThread);
        }
    });

    std::string cmdline = cmdline_of(cmd);

    LOG_INFO("| -> Catter-Proxy Final Executing command: \n    exe = {} \n    args = {}",
             cmd.executable,
             cmdline);

    auto ret = CreateProcessA(cmd.executable.c_str(),
                              cmdline.data(),
                              nullptr,
                              nullptr,
                              FALSE,
                              CREATE_SUSPENDED,
                              env_block.data(),
                              cmd.cwd.empty() ? nullptr : cmd.cwd.c_str(),
                              &si,
                              &pi);

    if(!ret) {
        // Error see
        // https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessa
        throw std::system_error(GetLastError(), std::system_category(), "Failed to create process");
    }

    std::filesystem::path dll_path = catter::util::get_catter_root_path() / win::DLL_NAME;

    if(!try_inject(pi.hProcess, catter::util::get_catter_root_path() / win::DLL_NAME)) {
        throw std::runtime_error("Failed to inject DLL into target process");
    }

    if(ResumeThread(pi.hThread) == static_cast<DWORD>(-1)) {
        throw std::system_error(GetLastError(),
                                std::system_category(),
                                "Failed to resume target process");
    }

    if(auto error = win::wait_for_object(pi.hProcess); error) {
        throw std::system_error(error.value(),
                                std::system_category(),
                                "Failed to wait for target process");
    }

    DWORD exit_code = 0;

    if(GetExitCodeProcess(pi.hProcess, &exit_code) == FALSE) {
        throw std::system_error(GetLastError(),
                                std::system_category(),
                                "Failed to get exit code of process");
    }
    return static_cast<int64_t>(exit_code);
};
};  // namespace catter::proxy::hook
