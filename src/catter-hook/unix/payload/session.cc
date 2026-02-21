#include "session.h"

#include "debug.h"
#include "environment.h"
#include "unix/config.h"
#include <string>

namespace {
std::string proxy_path_string = "";
std::string self_id_string = "";
}  // namespace

namespace catter::session {

void from(Session& session, const char** environment) noexcept {
    auto proxy_path = catter::env::get_env_value(environment, config::hook::KEY_CATTER_PROXY_PATH);
    if(proxy_path == nullptr) {
        WARN("catter proxy path not found in environment");
        proxy_path_string = "";
        return;
    } else {
        proxy_path_string = proxy_path;
    }
    auto self_id = catter::env::get_env_value(environment, config::hook::KEY_CATTER_COMMAND_ID);
    if(self_id == nullptr) {
        WARN("catter self id not found in environment");
        self_id_string = "";
        return;
    } else {
        self_id_string = self_id;
    }
    session.proxy_path = proxy_path_string;
    session.self_id = self_id_string;
    if(!is_valid(session)) {
        WARN("session is invalid");
        return;
    }

    INFO("session from env: catter_proxy={}, self_id={}", session.proxy_path, session.self_id);
}

bool is_valid(const Session& session) noexcept {
    return (!session.proxy_path.empty() && !session.self_id.empty());
}
}  // namespace catter::session
