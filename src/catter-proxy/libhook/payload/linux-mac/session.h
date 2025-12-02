#pragma once

namespace catter {

class Buffer;

/**
 * Represents an intercept session parameter set.
 *
 * It does not own the memory (of the pointed areas).
 */
struct Session {
    /// we execute proxy_path instead of cmd
    const char* proxy_path = nullptr;
    /// we pass self_id to proxy_path
    const char* self_id = nullptr;
    const char* necessary_envp_entry[2] = {nullptr, nullptr};
};

namespace session {

// Util method to initialize instance.
void from(Session& session, const char** environment) noexcept;

// Util method to store the values.
void persist(Session& session, char* begin, char* end) noexcept;

// Util method to check if session is initialized.
bool is_valid(const Session& session) noexcept;
}  // namespace session
}  // namespace catter
