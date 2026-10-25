#pragma once

#include <string>

namespace catter::hook {

/// Represents an intercept session parameter set.
///
/// It does not own the memory (of the pointed areas).
struct Session {
    std::string proxy_path{};
    std::string self_id{};
    /// The pipe of the catter session the proxies report to.
    std::string ipc{};

    static Session make(const char* const envp[]) noexcept;

    bool is_valid() noexcept {
        return (!proxy_path.empty() && !self_id.empty() && !ipc.empty());
    }
};
}  // namespace catter::hook
