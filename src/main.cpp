#include <cstddef>
#include <string>
#include <filesystem>
#include <vector>
#include <format>
#include <iostream>
#include <ranges>
#include <fstream>
#include <print>


#include <windows.h>
#include <detours.h>

#include "common.h"


std::string error(DWORD code) {
    LPSTR buffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&buffer,
        0,
        NULL);
    std::string message(buffer, size);
    LocalFree(buffer);
    return message;
}

std::vector<std::string> collect_all(){
    std::vector<std::string> result;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(catter::capture_root)) {
        if (entry.is_regular_file()){
            std::ifstream ifs(entry.path(), std::ios::in | std::ios::binary);
            std::string line;
            while (std::getline(ifs, line)) {
                result.push_back(line);
            }
        }
    }

    std::filesystem::remove_all(catter::capture_root);

    return result;
}

int attach_run(std::string_view command_line) {
    PROCESS_INFORMATION pi{};
    STARTUPINFOA si{
        .cb = sizeof(STARTUPINFOA)
    };

    auto ret = ::DetourCreateProcessWithDllExA(
        nullptr,
        const_cast<LPSTR>(command_line.data()),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_CONSOLE,
        nullptr,
        nullptr,
        &si,
        &pi,
        catter::hook_dll,
        nullptr
    );

    if (ret == FALSE) {
       std::println("Failed to create process with hook. Error: {} ({})", GetLastError(), error(GetLastError()));
        return -1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;

    if (GetExitCodeProcess(pi.hProcess, &exit_code) == FALSE) {
        std::println("Failed to get exit code. Error: {} ({})", GetLastError(), error(GetLastError()));
        return -1;
    }
    return static_cast<int>(exit_code);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::println("Usage: {} <command>", argv[0]);
        return 1;
    }

    std::string command_line;
    for (auto i : std::views::iota(1, argc)) {
        if (!command_line.empty()) {
            command_line += " ";
        }
        command_line += argv[i];
    }

    int result = attach_run(command_line);
    if (result != 0) {
        std::println("Process failed with exit code: {}", result);
    }

    auto captured_output = collect_all();
    for (const auto& line : captured_output) {
        std::println("{}", line);
    }
    
    return 0;
}
