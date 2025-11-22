#include <expected>
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

#include "env.h"
#include "interface/hook.h"

namespace catter::hook {

int run(Command cmd) {
    PROCESS_INFORMATION pi{};
    STARTUPINFOA si{.cb = sizeof(STARTUPINFOA)};

    std::filesystem::path dll_path = catter::win::current_path() / catter::win::dll_name;

    std::string cmdline = cmd.cmdline();

    auto ret = DetourCreateProcessWithDllExA(nullptr,
                                             cmdline.data(),
                                             nullptr,
                                             nullptr,
                                             FALSE,
                                             0,
                                             nullptr,
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

};  // namespace catter::hook
