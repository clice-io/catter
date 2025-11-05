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
 * Replaces the value for the given environment name, from the given
 * environment array.
 *
 * It's a re-implementation of the standard library function..
 *
 * @param envp the environment array.
 * @param key the name of the environment.
 * @param new_key_eq_value the new "key=value" string to set.
 * @return 0 on success, -1 on failure.
 */
const int replace_env_value(char** envp,
                            const char* const key,
                            const char* new_key_eq_value) noexcept;
}  // namespace catter::env
