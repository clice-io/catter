#pragma once

#include <list>
#include <string>
#include <vector>

namespace catter {

struct SanitizedEnv {
    std::vector<char*> entries;
    std::list<std::string> owned_entries;

    [[nodiscard]]
    char* const* data() noexcept {
        return entries.data();
    }
};

/// Remove envs used by hook so the target process is not affected by them.
[[nodiscard]]
SanitizedEnv sanitize_environment(char* const envp[]) noexcept;
}  // namespace catter
