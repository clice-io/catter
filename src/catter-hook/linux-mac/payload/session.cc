#include "session.h"

#include "linux-mac/debug.h"
#include "environment.h"
#include "linux-mac/config.h"
#include <string>

namespace {
std::string proxy_path_string;
std::string self_id_string;
}  // namespace

namespace catter::session {

void from(Session& session, const char** environment) noexcept {
    proxy_path_string =
        catter::env::get_env_value(environment, config::hook::KEY_CATTER_PROXY_PATH);
    self_id_string = catter::env::get_env_value(environment, config::hook::KEY_CATTER_COMMAND_ID);
    session.proxy_path = proxy_path_string;
    session.self_id = self_id_string;
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

bool is_valid(const Session& session) noexcept {
    return (!session.proxy_path.empty() && !session.self_id.empty());
}
}  // namespace catter::session
