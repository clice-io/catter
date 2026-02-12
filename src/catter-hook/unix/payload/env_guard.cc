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
            return nullptr;

        auto value = full_entry.substr(eq_pos + 1);
        std::string new_value;
        for(const auto& lib: value | std::views::split(catter::config::OS_PATH_SEPARATOR)) {
            std::string_view lib_sv(lib.begin(), lib.end());
            if(lib_sv.ends_with(catter::config::hook::HOOK_LIB_NAME)) {
                continue;
            }
            if(!new_value.empty()) {
                new_value += catter::config::OS_PATH_SEPARATOR;
            }
            new_value += lib_sv;
        }
        if(new_value.empty()) {
            // If the new value is empty, we just skip this entry.
            return nullptr;
        }
        new_preload.push_back(catter::config::hook::LD_PRELOAD_INIT_ENTRY + new_value);
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
