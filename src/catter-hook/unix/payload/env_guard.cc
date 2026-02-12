#include "env_guard.h"
#include "unix/config.h"
#include "unix/payload/environment.h"
#include <cstddef>
#include <cstring>
#include <list>
#include <ranges>
#include <string_view>

namespace {
const char* handle_env(std::list<std::string>& new_preload, char* entry) noexcept {
    for(const auto& key: catter::config::hook::KEYS_TO_INJECT) {
        if(catter::env::is_entry_of(entry, key)) {
            return nullptr;
        }
    }

    if(catter::env::is_entry_of(entry, catter::config::hook::KEY_PRELOAD)) {
        std::string_view full_entry(entry);
        size_t eq_pos = full_entry.find('=');
        if(eq_pos == std::string_view::npos)
            return entry;

        auto value = full_entry.substr(eq_pos + 1);
        new_preload.push_back(catter::config::hook::LD_PRELOAD_INIT_ENTRY +
                              (std::views::split(value, catter::config::OS_PATH_SEPARATOR) |
                               std::views::filter([](auto lib) {
                                   return !std::string_view(lib).ends_with(
                                       catter::config::hook::HOOK_LIB_NAME);
                               }) |
                               std::views::join_with(catter::config::OS_PATH_SEPARATOR) |
                               std::ranges::to<std::string>()));
        return new_preload.back().c_str();
    }
    return entry;
}
}  // namespace

namespace catter {
EnvGuard::EnvGuard(const char*** env_ptr) noexcept {
    new_envs_.reserve(64);

    for(size_t i = 0; (*env_ptr)[i] != nullptr; ++i) {
        char* env = const_cast<char*>((*env_ptr)[i]);
        if(auto new_entry = handle_env(new_preload_, env); new_entry != nullptr) {
            new_envs_.push_back(const_cast<char*>(new_entry));
        }
    }
    new_envs_.push_back(nullptr);

    *env_ptr = const_cast<const char**>(new_envs_.data());
}
}  // namespace catter
