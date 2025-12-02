#pragma once

namespace catter::env {

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
const char* get_env_value(const char** envp, const char* key) noexcept;

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
const char* get_env_entry(const char** envp, const char* const key) noexcept;

}  // namespace catter::env
