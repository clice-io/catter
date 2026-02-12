#pragma once

#include <string_view>

namespace catter {

class Buffer;

/**
 * Represents an intercept session parameter set.
 *
 * It does not own the memory (of the pointed areas).
 */
struct Session {
    std::string_view proxy_path{};
    std::string_view self_id{};
};

namespace session {

// Util method to initialize instance.
void from(Session& session, const char** environment) noexcept;

// Util method to check if session is initialized.
bool is_valid(const Session& session) noexcept;
}  // namespace session
}  // namespace catter
