#pragma once

namespace catter {

class Buffer;

/**
 * Represents an intercept session parameter set.
 *
 * It does not own the memory (of the pointed areas).
 */
struct Session {
    const char* log_path = nullptr;
    const char* self_lib_path = nullptr;
    const char* neccessary_envp_entry[2] = {nullptr, nullptr};
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
