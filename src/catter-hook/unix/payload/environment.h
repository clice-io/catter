#pragma once

#include <string_view>

namespace catter::env {

bool is_entry_of(const char* entry, std::string_view key) noexcept;

/**
 * Returns the value for the given environment name, from the given
 * environment array.
 *
 * It's a re-implementation of the standard library function..
 *
 * @param envp the environment array.
 * @param key the name of the environment.
 * @return the value of the environment.
 */
const char* get_env_value(const char** envp, std::string_view key) noexcept;

/**
 * Returns the entry for the given environment name, from the given
 * environment array.
 *
 * It's a re-implementation of the standard library function..
 *
 * @param envp the environment array.
 * @param key the name of the environment.
 * @return the entry of the environment.
 */
const char* get_env_entry(const char** envp, std::string_view key) noexcept;

}  // namespace catter::env
