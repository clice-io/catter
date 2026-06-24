#pragma once

#include <string>

namespace catter {

/**
 * Represents an intercept session parameter set.
 *
 * It does not own the memory (of the pointed areas).
 */
struct Session {
    std::string proxy_path{};
    std::string self_id{};

    static Session make(const char* const envp[]) noexcept;

    bool is_valid() noexcept {
        return (!proxy_path.empty() && !self_id.empty());
    }
};
}  // namespace catter
