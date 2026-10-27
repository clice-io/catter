#include "hook/unix/environment.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#ifdef CATTER_MAC
#include <crt_externs.h>
#else
extern char** environ;
#endif

#include "support/hook.h"
#include "support/messages.h"

namespace catter::hook {

namespace {

bool is_entry_of(std::string_view entry, std::string_view name) noexcept {
    return entry.size() > name.size() && entry.starts_with(name) && entry[name.size()] == '=';
}

/// The entry of PRELOAD `entry` without the hook; none when it preloads
/// nothing else.
std::optional<std::string> preload_without_hook(std::string_view entry) {
    std::string value;
    auto rest = entry.substr(entry.find('=') + 1);
    while(!rest.empty()) {
        const auto end = rest.find_first_of(PRELOAD_SEPARATORS);
        const auto library = rest.substr(0, end);
        rest.remove_prefix(end == std::string_view::npos ? rest.size() : end + 1);
        if(library.empty() || library.ends_with(LIBRARY)) {
            continue;
        }
        if(!value.empty()) {
            value += PRELOAD_SEPARATORS[0];
        }
        value += library;
    }
    if(value.empty()) {
        return std::nullopt;
    }
    return std::string(PRELOAD) + "=" + value;
}

}  // namespace

char** environment() noexcept {
#ifdef CATTER_MAC
    return *_NSGetEnviron();
#else
    return environ;
#endif
}

const char* get_env_entry(const char* const envp[], std::string_view name) noexcept {
    for(auto entry = envp; *entry != nullptr; ++entry) {
        if(is_entry_of(*entry, name)) {
            return *entry;
        }
    }
    return nullptr;
}

const char* get_env_value(const char* const envp[], std::string_view name) noexcept {
    auto entry = get_env_entry(envp, name);
    return entry == nullptr ? nullptr : entry + name.size() + 1;
}

Environment proxy_environment(char* const envp[], std::span<std::string> session) {
    Environment env;
    for(auto entry = envp; entry != nullptr && *entry != nullptr; ++entry) {
        if(std::ranges::any_of(env::VARIABLES,
                               [&](std::string_view name) { return is_entry_of(*entry, name); })) {
            continue;
        }
        if(!is_entry_of(*entry, PRELOAD)) {
            env.entries.push_back(*entry);
        } else if(auto preload = preload_without_hook(*entry)) {
            env.entries.push_back(env.owned.emplace_back(std::move(*preload)).data());
        }
    }
    for(auto& entry: session) {
        env.entries.push_back(entry.data());
    }
    env.entries.push_back(nullptr);
    return env;
}

}  // namespace catter::hook
