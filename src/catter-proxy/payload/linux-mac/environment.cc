#include "environment.h"
#include "array.h"
#include "linux-mac/debug.h"

namespace catter::env {

const char* get_env_value(const char** envp, const char* const key) noexcept {
    const size_t key_size = catter::array::length(key);
    INFO("getting env value for key: {}", key);

    for(const char** it = envp; *it != nullptr; ++it) {
        const char* const current = *it;
        // Is the key a prefix of the pointed string?
        if(!catter::array::equal_n(key, current, key_size))
            continue;
        // Is the next character is the equal sign?
        if(current[key_size] != '=')
            continue;
        // It must be the one! Calculate the address of the value.
        INFO("env key: {} found, value: {}", key, current + key_size + 1);
        return current + key_size + 1;
    }
    INFO("env key: {} not found", key);
    return nullptr;
}

const char* get_env_entry(const char** envp, const char* const key) noexcept {
    const size_t key_size = catter::array::length(key);
    INFO("getting env entry for key: {}", key);

    for(const char** it = envp; *it != nullptr; ++it) {
        const char* const current = *it;
        // Is the key a prefix of the pointed string?
        if(!catter::array::equal_n(key, current, key_size))
            continue;
        // Is the next character is the equal sign?
        if(current[key_size] != '=')
            continue;
        // It must be the one! Return the entry.
        return current;
    }
    INFO("env entry for key: {} not found", key);
    return nullptr;
}

}  // namespace catter::env
