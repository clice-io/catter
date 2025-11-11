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

#include "hook/interface.h"
#include "hook/windows/env.h"

namespace catter::hook {

namespace fs = std::filesystem;

namespace detail {
std::string quote_win32_arg(std::string_view arg) {
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
}  // namespace detail

int run(std::span<const char* const> command, std::error_code& ec) {
    if(command.empty()) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return 0;
    }

    std::string command_line;

    auto view = command | std::views::transform([](auto&& s) {
                    return std::string_view{s, std::char_traits<char>::length(s)};
                }) |
                std::views::transform(detail::quote_win32_arg);

    bool space = false;
    for(auto&& c: view) {
        if(space) {
            command_line.push_back(' ');
        }
        command_line.append(c);
        space = true;
    }

    PROCESS_INFORMATION pi{};
    STARTUPINFOA si{.cb = sizeof(STARTUPINFOA)};

    auto [length, path] = catter::win::path();

    std::filesystem::path dll_path{path.get()};
    dll_path.replace_filename(catter::win::hook_dll_name);

    auto ret = DetourCreateProcessWithDllExA(nullptr,
                                             command_line.data(),
                                             nullptr,
                                             nullptr,
                                             FALSE,
                                             CREATE_NEW_CONSOLE,
                                             nullptr,
                                             nullptr,
                                             &si,
                                             &pi,
                                             dll_path.string().c_str(),
                                             nullptr);

    if(!ret) {
        // Error see
        // https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessa
        ec = std::error_code(GetLastError(), std::system_category());
        return 0;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code = 0;

    if(GetExitCodeProcess(pi.hProcess, &exit_code) == FALSE) {
        ec = std::error_code(GetLastError(), std::system_category());
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 0;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    ec.clear();
    return static_cast<int>(exit_code);
}

std::expected<std::vector<std::string>, std::string> collect_all() {
    std::vector<std::string> result;

    std::error_code ec;

    if(!fs::exists(catter::win::capture_root, ec)) {
        return result;
    }

    if(ec) {
        std::println("Failed to access capture root directory: {}: {}",
                     catter::win::capture_root,
                     ec.message());
        return result;
    }

    auto dir_iter = fs::recursive_directory_iterator(catter::win::capture_root,
                                                     fs::directory_options::skip_permission_denied,
                                                     ec);

    for(; dir_iter != fs::end(dir_iter); dir_iter.increment(ec)) {
        if(ec) {
            std::println("Failed to access directory entry: {}: {}",
                         dir_iter->path().string(),
                         ec.message());
            ec.clear();
            continue;
        }

        const auto& entry = *dir_iter;

        if(entry.is_regular_file()) {
            std::ifstream ifs(entry.path(), std::ios::in | std::ios::binary);
            if(!ifs) {
                std::println("Failed to open file: {}", entry.path().string());
                continue;
            }
            std::string line;
            while(std::getline(ifs, line)) {
                result.push_back(line);
            }
        }
    }

    fs::remove_all(catter::win::capture_root, ec);
    if(ec) {
        std::println("Failed to remove capture root directory: {}: {}",
                     catter::win::capture_root,
                     ec.message());
    }
    return result;
}

};  // namespace catter::hook
