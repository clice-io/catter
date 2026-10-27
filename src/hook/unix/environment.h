#pragma once

#include <list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace catter::hook {

/// This process's environment, as the C library keeps it.
char** environment() noexcept;

/// The value of variable `name` in `envp`; null when it has none.
const char* get_env_value(const char* const envp[], std::string_view name) noexcept;

/// The entry of variable `name` in `envp`, `name=value`; null when it has none.
const char* get_env_entry(const char* const envp[], std::string_view name) noexcept;

/// An environment as catter-proxy gets it, which runs without the hook.
struct Environment {
    std::vector<char*> entries;

    /// The entries rewritten, which `entries` point into.
    std::list<std::string> owned;

    char* const* data() noexcept {
        return entries.data();
    }
};

/// `envp` as catter-proxy gets it: without the hook among the libraries the
/// loader preloads, and with `session`, the entries of the variables that name
/// the session, in place of those it has: a program may start another with an
/// environment of its own making, as `env -i` does.
Environment proxy_environment(char* const envp[], std::span<std::string> session);

}  // namespace catter::hook
