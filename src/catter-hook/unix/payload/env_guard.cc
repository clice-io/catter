#include "env_guard.h"
#include "unix/config.h"
#include "unix/payload/environment.h"
#include "unix/payload/session.h"
#include <cstddef>
#include <cstring>
#include <print>
#include <ranges>
#include <string_view>

namespace {
bool handle_env(char* entry) noexcept {
    for(const auto& key: catter::config::hook::KEYS_TO_INJECT) {
        if(catter::env::is_entry_of(entry, key)) {
            return false;
        }
    }

    if(catter::env::is_entry_of(entry, catter::config::hook::KEY_PRELOAD)) {
        std::string_view full_entry(entry);
        size_t eq_pos = full_entry.find('=');
        if(eq_pos == std::string_view::npos)
            return true;

        auto value = full_entry.substr(eq_pos + 1);
        char* write_ptr = entry + eq_pos + 1;
        size_t current_offset = 0;
        bool first = true;

        for(auto part: std::ranges::views::split(value, catter::config::OS_PATH_SEPARATOR)) {
            std::string_view path_sv(part);

            if(!path_sv.ends_with(catter::config::hook::RELATIVE_PATH_OF_HOOK_LIB)) {
                if(!first) {
                    write_ptr[current_offset++] = catter::config::OS_PATH_SEPARATOR;
                }
                std::memmove(write_ptr + current_offset, path_sv.data(), path_sv.size());
                current_offset += path_sv.size();
                first = false;
            }
        }
        write_ptr[current_offset] = '\0';
    }
    return true;
}
}  // namespace

namespace catter {
EnvGuard::EnvGuard(const char*** env_ptr) noexcept {
    new_envs.reserve(64);

    for(size_t i = 0; (*env_ptr)[i] != nullptr; ++i) {
        char* env = const_cast<char*>((*env_ptr)[i]);
        if(handle_env(env)) {
            new_envs.push_back(env);
        }
    }
    new_envs.push_back(nullptr);

    *env_ptr = const_cast<const char**>(new_envs.data());
}
}  // namespace catter
