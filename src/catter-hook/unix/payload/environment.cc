#include "environment.h"
#include "debug.h"
#include <cstddef>
#include <string_view>

namespace catter::env {

bool is_entry_of(const char* entry, std::string_view key) noexcept {
    std::string_view sv = entry;
    return sv.starts_with(key) && sv.size() > key.size() && sv[key.size()] == '=';
}

const char* get_env_value(const char** envp, std::string_view key) noexcept {
    const std::size_t key_size = key.size();
    INFO("getting env value for key: {}", key);

    for(const char** it = envp; *it != nullptr; ++it) {
        if(!is_entry_of(*it, key))
            continue;
        // It must be the one! Calculate the address of the value.
        INFO("env key: {} found, value: {}", key, *it + key_size + 1);
        return *it + key_size + 1;
    }
    INFO("env key: {} not found", key);
    return nullptr;
}

const char* get_env_entry(const char** envp, std::string_view key) noexcept {
    const size_t key_size = key.size();
    INFO("getting env entry for key: {}", key);

    for(const char** it = envp; *it != nullptr; ++it) {
        if(!is_entry_of(*it, key))
            continue;
        return *it;
    }
    INFO("env entry for key: {} not found", key);
    return nullptr;
}

}  // namespace catter::env
