#include "session.h"

#include "buffer.h"
#include "linux-mac/debug.h"
#include "environment.h"
#include "config/linux-mac-hook.h"

namespace catter::session {

void from(Session& session, const char** environment) noexcept {
    session.proxy_path =
        catter::env::get_env_value(environment, config::hook::KEY_CATTER_PROXY_PATH);
    session.self_id = catter::env::get_env_value(environment, config::hook::KEY_CATTER_COMMAND_ID);
    if(!is_valid(session)) {
        WARN("session is invalid");
        return;
    }
    session.necessary_envp_entry[0] =
        catter::env::get_env_entry(environment, config::hook::KEY_CATTER_PROXY_PATH);
    session.necessary_envp_entry[1] =
        catter::env::get_env_entry(environment, config::hook::KEY_CATTER_COMMAND_ID);

    INFO("session from env: catter_proxy={}, self_id={}", session.proxy_path, session.self_id);
}

void persist(Session& session, char* begin, char* end) noexcept {
    if(!is_valid(session))
        return;

    Buffer buffer(begin, end);
    session.proxy_path = buffer.store(session.proxy_path);
    session.self_id = buffer.store(session.self_id);
    session.necessary_envp_entry[0] = buffer.store(session.necessary_envp_entry[0]);
    session.necessary_envp_entry[1] = buffer.store(session.necessary_envp_entry[1]);
}

bool is_valid(const Session& session) noexcept {
    return (session.proxy_path != nullptr && session.self_id != nullptr);
}
}  // namespace catter::session
