#include "session.h"

#include <string>

#include "debug.h"
#include "environment.h"
#include "unix/config.h"

namespace catter {

Session Session::make(const char** environment) noexcept {
    Session session;
    auto proxy_path = catter::env::get_env_value(environment, config::hook::KEY_CATTER_PROXY_PATH);
    if(proxy_path == nullptr) {
        WARN("catter proxy path not found in environment");
        return session;
    } else {
        session.proxy_path = proxy_path;
    }
    auto self_id = catter::env::get_env_value(environment, config::hook::KEY_CATTER_COMMAND_ID);
    if(self_id == nullptr) {
        WARN("catter self id not found in environment");
        return session;
    } else {
        session.self_id = self_id;
    }
    if(!session.is_valid()) {
        WARN("session is invalid");
        return session;
    }

    INFO("session from env: catter_proxy={}, self_id={}", session.proxy_path, session.self_id);
    return session;
}
}  // namespace catter
