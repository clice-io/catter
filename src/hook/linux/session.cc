#include "session.h"

#include "buffer.h"
#include "environment.h"
#include "config.h"

namespace catter::session {

void from(Session& session, const char** environment) noexcept {
    if(nullptr == environment)
        return;

    session.log_file = catter::env::get_env_value(environment, config::KEY_CMD_LOG_FILE);
    session.self_lib_path =
        catter::env::get_env_value(environment, config::KEY_CATTER_PRELOAD_PATH);
}

void persist(Session& session, char* begin, char* end) noexcept {
    if(!is_valid(session))
        return;

    Buffer buffer(begin, end);
    session.log_file = buffer.store(session.log_file);
    session.self_lib_path = buffer.store(session.self_lib_path);
}

bool is_valid(const Session& session) noexcept {
    return (session.log_file != nullptr && session.self_lib_path != nullptr);
}
}  // namespace catter::session
