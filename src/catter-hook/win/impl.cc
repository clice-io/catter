#include <string>
#include <system_error>
#include <filesystem>
#include <vector>
#include <cassert>
#include <cstdint>
#include <string>
#include <stdexcept>

#include <windows.h>
#include <libloaderapi.h>
#include <minwindef.h>
#include <Psapi.h>
#include <detours.h>

#include "util/log.h"
#include "util/data.h"
#include "util/crossplat.h"

#include "win/env.h"
#include <string>

namespace {

HANDLE RtlCreateUserThread(HANDLE hProcess, LPVOID lpBaseAddress, LPVOID lpSpace) {
    // undocumented.ntinternals.com
    typedef DWORD(WINAPI * functypeRtlCreateUserThread)(HANDLE ProcessHandle,
                                                        PSECURITY_DESCRIPTOR SecurityDescriptor,
                                                        BOOL CreateSuspended,
                                                        ULONG StackZeroBits,
                                                        PULONG StackReserved,
                                                        PULONG StackCommit,
                                                        LPVOID StartAddress,
                                                        LPVOID StartParameter,
                                                        HANDLE ThreadHandle,
                                                        LPVOID ClientID);
    HANDLE hRemoteThread = NULL;
    HMODULE hNtDllModule = GetModuleHandle("ntdll.dll");
    if(hNtDllModule == NULL) {
        return NULL;
    }
    functypeRtlCreateUserThread funcRtlCreateUserThread =
        (functypeRtlCreateUserThread)GetProcAddress(hNtDllModule, "RtlCreateUserThread");
    if(!funcRtlCreateUserThread) {
        return NULL;
    }
    funcRtlCreateUserThread(hProcess,
                            NULL,
                            0,
                            0,
                            0,
                            0,
                            lpBaseAddress,
                            lpSpace,
                            &hRemoteThread,
                            NULL);
    DWORD lastError = GetLastError();
    if(lastError)
        throw std::runtime_error(std::to_string(lastError));
    return hRemoteThread;
}

HANDLE NtCreateThreadEx(HANDLE hProcess, LPVOID lpBaseAddress, LPVOID lpSpace) {
    // undocumented.ntinternals.com
    typedef DWORD(WINAPI * functypeNtCreateThreadEx)(PHANDLE ThreadHandle,
                                                     ACCESS_MASK DesiredAccess,
                                                     LPVOID ObjectAttributes,
                                                     HANDLE ProcessHandle,
                                                     LPTHREAD_START_ROUTINE lpStartAddress,
                                                     LPVOID lpParameter,
                                                     BOOL CreateSuspended,
                                                     DWORD dwStackSize,
                                                     DWORD Unknown1,
                                                     DWORD Unknown2,
                                                     LPVOID Unknown3);
    HANDLE hRemoteThread = NULL;
    HMODULE hNtDllModule = NULL;
    functypeNtCreateThreadEx funcNtCreateThreadEx = NULL;
    hNtDllModule = GetModuleHandle("ntdll.dll");
    if(hNtDllModule == NULL) {
        return NULL;
    }
    funcNtCreateThreadEx =
        (functypeNtCreateThreadEx)GetProcAddress(hNtDllModule, "NtCreateThreadEx");
    if(!funcNtCreateThreadEx) {
        return NULL;
    }
    funcNtCreateThreadEx(&hRemoteThread,
                         GENERIC_ALL,
                         NULL,
                         hProcess,
                         (LPTHREAD_START_ROUTINE)lpBaseAddress,
                         lpSpace,
                         FALSE,
                         NULL,
                         NULL,
                         NULL,
                         NULL);
    return hRemoteThread;
}

enum class InjectMethod { CreateRemoteThread, NtCreateThread, RtlCreateUserThread };

template <InjectMethod method>
HANDLE inject(HANDLE hProcess, const std::string& dll_path) {
    LPVOID lpSpace = (LPVOID)VirtualAllocEx(hProcess,
                                            NULL,
                                            dll_path.length(),
                                            MEM_RESERVE | MEM_COMMIT,
                                            PAGE_EXECUTE_READWRITE);
    if(!lpSpace)
        throw std::runtime_error("failed to allocate memory in process");

    int n = WriteProcessMemory(hProcess, lpSpace, dll_path.c_str(), dll_path.length(), NULL);
    if(n == 0)
        throw std::runtime_error("failed to write into process");

    switch(method) {
        case InjectMethod::NtCreateThread:
            return NtCreateThreadEx(hProcess, (void*)LoadLibraryA, lpSpace);
        case InjectMethod::RtlCreateUserThread:
            return RtlCreateUserThread(hProcess, (void*)LoadLibraryA, lpSpace);
        default:
            return CreateRemoteThread(hProcess,
                                      NULL,
                                      0,
                                      (LPTHREAD_START_ROUTINE)(void*)LoadLibraryA,
                                      lpSpace,
                                      NULL,
                                      NULL);
    }
}

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

std::string cmdline_of(const catter::data::command& cmd) noexcept {
    std::string full_cmd;
    for(const auto& arg: cmd.args) {
        full_cmd += quote_win32_arg(arg) + " ";
    }
    return full_cmd;
}
}  // namespace

namespace catter::proxy::hook {

int64_t run(data::command cmd, data::ipcid_t id, std::string proxy_path) {

    LOG_INFO("new command id is: {}", id);

    SetEnvironmentVariableA(catter::win::ENV_VAR_IPC_ID<char>, std::to_string(id).c_str());
    SetEnvironmentVariableA(catter::win::ENV_VAR_PROXY_PATH<char>, proxy_path.c_str());

    std::vector<char> env_block;

    for(auto c: catter::util::get_environment()) {
        std::span<const char> span(c.c_str(), c.size() + 1);
        env_block.append_range(span);
    }

    env_block.push_back('\0');  // Double null termination

    PROCESS_INFORMATION pi{};
    STARTUPINFOA si{.cb = sizeof(STARTUPINFOA)};

    std::filesystem::path dll_path = catter::util::get_catter_root_path() / catter::win::DLL_NAME;

    std::string cmdline = cmdline_of(cmd);

    LOG_INFO("| -> Catter-Proxy Final Executing command: \n    exe = {} \n    args = {}",
             cmd.executable,
             cmdline);

    auto ret = DetourCreateProcessWithDllExA(cmd.executable.c_str(),
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
    return static_cast<int64_t>(exit_code);
};

};  // namespace catter::proxy::hook
