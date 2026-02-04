#include "environment.h"
#include "linux-mac/debug.h"
#include <cstddef>
#include <string_view>

namespace catter::env {

const char* get_env_value(const char** envp, std::string_view key) noexcept {
    const std::size_t key_size = key.size();
    INFO("getting env value for key: {}", key);

    for(const char** it = envp; *it != nullptr; ++it) {
        std::string_view current = *it;
        // Is the key a prefix of the pointed string?
        if(!current.starts_with(key))
            continue;
        // Is the next character is the equal sign?
        if(current.size() <= key_size || current[key_size] != '=')
            continue;
        // It must be the one! Calculate the address of the value.
        INFO("env key: {} found, value: {}", key, current.data() + key_size + 1);
        return current.data() + key_size + 1;
    }
    INFO("env key: {} not found", key);
    return nullptr;
}

const char* get_env_entry(const char** envp, std::string_view key) noexcept {
    const size_t key_size = key.size();
    INFO("getting env entry for key: {}", key);

    for(const char** it = envp; *it != nullptr; ++it) {
        std::string_view current = *it;
        // Is the key a prefix of the pointed string?
        if(!current.starts_with(key))
            continue;
        // Is the next character is the equal sign?
        if(current.size() <= key_size || current[key_size] != '=')
            continue;
        // It must be the one! Return the entry.
        return current.data();
    }
    INFO("env entry for key: {} not found", key);
    return nullptr;
}

}  // namespace catter::env
