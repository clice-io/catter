#include "env_sanitizer.h"

#include <cstddef>
#include <cstring>
#include <list>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>

#include "unix/config.h"
#include "unix/payload/environment.h"

namespace {
bool should_drop_entry(const char* entry) noexcept {
    for(const auto& key: catter::config::hook::KEYS_TO_INJECT) {
        if(catter::env::is_entry_of(entry, key)) {
            return true;
        }
    }
    return false;
}

std::optional<std::string> sanitize_preload_entry(const char* entry) noexcept {
    std::string_view full_entry(entry);
    size_t eq_pos = full_entry.find('=');
    if(eq_pos == std::string_view::npos)
        return std::nullopt;

    auto value = full_entry.substr(eq_pos + 1);
    std::string new_value;
    for(const auto& lib: value | std::views::split(catter::config::OS_PATH_SEPARATOR)) {
        std::string_view lib_sv(lib.begin(), lib.end());
        if(lib_sv.ends_with(catter::config::hook::HOOK_LIB_NAME)) {
            continue;
        }
        if(!new_value.empty()) {
            new_value += catter::config::OS_PATH_SEPARATOR;
        }
        new_value += lib_sv;
    }
    if(new_value.empty()) {
        // If the new value is empty, we just skip this entry.
        return std::nullopt;
    }
    return catter::config::hook::LD_PRELOAD_INIT_ENTRY + new_value;
}
}  // namespace

namespace catter {
SanitizedEnv sanitize_environment(char* const envp[]) noexcept {
    SanitizedEnv env;
    env.entries.reserve(64);

    if(envp == nullptr) {
        env.entries.push_back(nullptr);
        return env;
    }

    for(size_t i = 0; envp[i] != nullptr; ++i) {
        if(should_drop_entry(envp[i])) {
            continue;
        }

        if(catter::env::is_entry_of(envp[i], catter::config::hook::KEY_PRELOAD)) {
            auto new_entry = sanitize_preload_entry(envp[i]);
            if(!new_entry.has_value()) {
                continue;
            }
            env.owned_entries.push_back(std::move(*new_entry));
            env.entries.push_back(env.owned_entries.back().data());
            continue;
        }
        env.entries.push_back(envp[i]);
    }
    env.entries.push_back(nullptr);

    return env;
}
}  // namespace catter
