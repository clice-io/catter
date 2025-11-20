#pragma once

#include <filesystem>
#include <vector>
#include <windows.h>

namespace catter::win {

constexpr static char exe_name[] = "catter-proxy.exe";
constexpr static char dll_name[] = "catter-hook64.dll";

inline std::filesystem::path current_path(HMODULE h = nullptr) {

    std::vector<char> data;
    data.resize(MAX_PATH);

    while(true) {
        if(GetModuleFileNameA(h, data.data(), data.size()) == data.size() &&
           GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            data.resize(data.size() * 2);
        } else {
            break;
        }
    }

    return std::filesystem::path(data.data()).parent_path();
}

}  // namespace catter::win
