#include "session.h"

#include "buffer.h"
#include "debug.h"
#include "environment.h"
#include "libhook/linux-mac/config.h"

namespace catter::session {

void from(Session& session, const char** environment) noexcept {
    session.log_path = catter::env::get_env_value(environment, config::KEY_CMD_LOG_FILE);
    session.self_lib_path =
        catter::env::get_env_value(environment, config::KEY_CATTER_PRELOAD_PATH);
    if(!is_valid(session)) {
        WARN("session is invalid");
        return;
    }
    session.necessary_envp_entry[0] =
        catter::env::get_env_entry(environment, config::KEY_CMD_LOG_FILE);
    session.necessary_envp_entry[1] =
        catter::env::get_env_entry(environment, config::KEY_CATTER_PRELOAD_PATH);

    INFO("session from env: log_path={}, self_lib_path={}",
         session.log_path,
         session.self_lib_path);
}

void persist(Session& session, char* begin, char* end) noexcept {
    if(!is_valid(session))
        return;

    Buffer buffer(begin, end);
    session.log_path = buffer.store(session.log_path);
    session.self_lib_path = buffer.store(session.self_lib_path);
    session.necessary_envp_entry[0] = buffer.store(session.necessary_envp_entry[0]);
    session.necessary_envp_entry[1] = buffer.store(session.necessary_envp_entry[1]);
}

bool is_valid(const Session& session) noexcept {
    return (session.log_path != nullptr && session.self_lib_path != nullptr);
}
}  // namespace catter::session
