#include "environment.h"
#include "array.h"

namespace catter::env {

const char* get_env_value(const char** envp, const char* const key) noexcept {
    const size_t key_size = catter::array::length(key);

    for(const char** it = envp; *it != nullptr; ++it) {
        const char* const current = *it;
        // Is the key a prefix of the pointed string?
        if(!catter::array::equal_n(key, current, key_size))
            continue;
        // Is the next character is the equal sign?
        if(current[key_size] != '=')
            continue;
        // It must be the one! Calculate the address of the value.
        return current + key_size + 1;
    }
    return nullptr;
}

const int replace_env_value(char** envp,
                            const char* const key,
                            const char* new_key_eq_value) noexcept {
    const size_t key_size = catter::array::length(key);

    for(char** it = envp; *it != nullptr; ++it) {
        const char* current = *it;
        // Is the key a prefix of the pointed string?
        if(!catter::array::equal_n(key, current, key_size))
            continue;
        // Is the next character is the equal sign?
        if(current[key_size] != '=')
            continue;
        // It must be the one! Replace the value.
        *it = const_cast<char*>(new_key_eq_value);
        return 0;
    }
    return -1;
}
}  // namespace catter::env
