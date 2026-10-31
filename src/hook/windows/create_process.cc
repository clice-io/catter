#include "hook/windows/create_process.h"

#include <algorithm>
#include <cstring>
#include <cwchar>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <windows.h>
#include <kotatsu/support/utf16.h>
#include <kotatsu/fs/resolve.h>
#include <kotatsu/fs/status.h>
#include <kotatsu/sys/env.h>
#include <kotatsu/sys/windows.h>

namespace catter::hook {

std::optional<std::string> resolve_program(const wchar_t* application_name,
                                           const wchar_t* command_line) {
    std::string found;
    if(application_name != nullptr && *application_name != L'\0') {
        kotatsu::from_utf16(found, application_name);
        if(!kotatsu::fs::is_file(found)) {
            return std::nullopt;
        }
    } else {
        std::string line;
        if(command_line != nullptr) {
            kotatsu::from_utf16(line, command_line);
        }
        auto words = kotatsu::sys::windows::split_command_line(line);
        auto first = words.begin();
        if(first == words.end()) {
            return std::nullopt;
        }
        auto directories = kotatsu::sys::windows::create_process_directories();
        if(!directories) {
            return std::nullopt;
        }
        // CreateProcessW reads a name without quotes up to each blank in turn,
        // until one names a file: C:\Program Files\tool.exe a b.
        std::string name(*first);
        auto end = name.size();
        while(kotatsu::sys::windows::find_create_process_program(found, name, *directories)) {
            if(line.starts_with('"') || end == line.size()) {
                return std::nullopt;
            }
            end = (std::min)(line.find_first_of(" \t", end + 1), line.size());
            name = line.substr(0, end);
        }
    }
    // Relative to this process's current directory, which the child's need
    // not be.
    std::string path;
    if(kotatsu::fs::absolute(path, found)) {
        return found;
    }
    return path;
}

std::vector<std::string> arguments_of(const wchar_t* application_name,
                                      const wchar_t* command_line) {
    std::string line;
    kotatsu::from_utf16(line, command_line != nullptr ? command_line : application_name);
    std::vector<std::string> arguments;
    for(auto argument: kotatsu::sys::windows::split_command_line(line)) {
        arguments.emplace_back(argument);
    }
    return arguments;
}

std::vector<std::string> environment_of(const void* block, bool unicode) {
    std::vector<std::string> variables;
    if(block == nullptr) {
        for(const auto& [name, value]: kotatsu::sys::environment()) {
            variables.push_back(std::format("{}={}", name, value));
        }
        return variables;
    }
    if(unicode) {
        for(auto* entry = static_cast<const wchar_t*>(block); *entry != L'\0';
            entry += std::wcslen(entry) + 1) {
            kotatsu::from_utf16(variables.emplace_back(), entry);
        }
        return variables;
    }
    for(auto* entry = static_cast<const char*>(block); *entry != '\0';
        entry += std::strlen(entry) + 1) {
        const int length = MultiByteToWideChar(CP_ACP, 0, entry, -1, nullptr, 0);
        std::wstring wide(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(CP_ACP, 0, entry, -1, wide.data(), length);
        wide.pop_back();
        kotatsu::from_utf16(variables.emplace_back(), wide);
    }
    return variables;
}

std::wstring environment_block(std::span<const std::string> variables) {
    std::vector<std::wstring> entries;
    for(const auto& variable: variables) {
        kotatsu::to_utf16(entries.emplace_back(), variable);
    }
    // A name ends at the first '=' past its first character: `=C:` is one.
    auto name = [](std::wstring_view entry) {
        return entry.substr(0, entry.find(L'=', 1));
    };
    std::ranges::stable_sort(entries, [&](std::wstring_view left, std::wstring_view right) {
        const auto a = name(left);
        const auto b = name(right);
        return CompareStringOrdinal(a.data(),
                                    static_cast<int>(a.size()),
                                    b.data(),
                                    static_cast<int>(b.size()),
                                    TRUE) == CSTR_LESS_THAN;
    });
    std::wstring block;
    for(const auto& entry: entries) {
        block += entry;
        block += L'\0';
    }
    // An empty block is two NULs.
    if(block.empty()) {
        block += L'\0';
    }
    block += L'\0';
    return block;
}

std::string directory_of(const wchar_t* directory) {
    std::string path;
    if(directory == nullptr) {
        kotatsu::fs::current_dir(path);
        return path;
    }
    kotatsu::from_utf16(path, directory);
    std::string absolute;
    if(kotatsu::fs::absolute(absolute, path)) {
        return path;
    }
    return absolute;
}

}  // namespace catter::hook
