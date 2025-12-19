#include <expected>
#include <format>
#include <print>
#include <string>
#include <ranges>
#include <system_error>
#include <span>
#include <fstream>
#include <filesystem>
#include <vector>

#include <windows.h>
#include <detours.h>

#include "uv/rpc_data.h"
#include "util/crossplat.h"

#include "win/env.h"
#include <string>

namespace {
std::string quote_win32_arg(std::string_view arg) noexcept {
    // No quoting needed if it's empty or has no special characters.
    if(arg.empty() || arg.find_first_of(" \t\n\v\"") == std::string_view::npos) {
        return std::string(arg);
    }

    std::string quoted_arg;
    quoted_arg.push_back('"');

    for(auto it = arg.begin();; ++it) {
        int num_backslashes = 0;
        while(it != arg.end() && *it == '\\') {
            ++it;
            ++num_backslashes;
        }

        if(it == arg.end()) {
            // End of string; append backslashes and a closing quote.
            quoted_arg.append(num_backslashes * 2, '\\');
            break;
        }

        if(*it == '"') {
            // Escape all backslashes and the following double quote.
            quoted_arg.append(num_backslashes * 2 + 1, '\\');
            quoted_arg.push_back(*it);
        } else {
            // Backslashes aren't special here.
            quoted_arg.append(num_backslashes, '\\');
            quoted_arg.push_back(*it);
        }
    }
    quoted_arg.push_back('"');
    return quoted_arg;
}

std::string cmdline_of(const catter::rpc::data::command& cmd) noexcept {
    std::string full_cmd = quote_win32_arg(cmd.executable);
    for(const auto& arg: cmd.args) {
        full_cmd += " " + quote_win32_arg(arg);
    }
    return full_cmd;
}

}  // namespace

namespace catter::proxy::hook {

int run(rpc::data::command cmd, rpc::data::command_id_t id) {
    std::string cmdline = cmdline_of(cmd);

    SetEnvironmentVariableA(catter::win::ENV_VAR_RPC_ID<char>, std::to_string(id).c_str());

    std::vector<char> env_block;

    for(auto c: catter::util::get_environment()) {
        std::span<const char> span(c.c_str(), c.size() + 1);
        env_block.append_range(span);
    }

    env_block.push_back('\0');  // Double null termination

    PROCESS_INFORMATION pi{};
    STARTUPINFOA si{.cb = sizeof(STARTUPINFOA)};

    std::filesystem::path dll_path = catter::util::get_catter_root_path() / catter::win::DLL_NAME;

    auto ret = DetourCreateProcessWithDllExA(nullptr,
                                             cmdline.data(),
                                             nullptr,
                                             nullptr,
                                             FALSE,
                                             0,
                                             env_block.data(),
                                             nullptr,
                                             &si,
                                             &pi,
                                             dll_path.string().c_str(),
                                             nullptr);

    if(!ret) {
        // Error see
        // https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessa
        throw std::system_error(GetLastError(),
                                std::system_category(),
                                "Failed to create process with injected dll");
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code = 0;

    if(GetExitCodeProcess(pi.hProcess, &exit_code) == FALSE) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        throw std::system_error(GetLastError(),
                                std::system_category(),
                                "Failed to get exit code of process");
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return static_cast<int>(exit_code);
};

void locate_exe(rpc::data::command& command) {
    return;
}

};  // namespace catter::proxy::hook
