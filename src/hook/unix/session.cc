#include "hook/unix/session.h"

#include <string>

#include "hook/unix/debug.h"
#include "hook/unix/environment.h"
#include "support/config/hook_unix.h"

namespace catter {

Session Session::make(const char* const envp[]) noexcept {
    Session session;
    auto proxy_path = catter::env::get_env_value(envp, config::hook::KEY_CATTER_PROXY_PATH);
    if(proxy_path == nullptr) {
        WARN("catter proxy path not found in environment");
        return session;
    } else {
        session.proxy_path = proxy_path;
    }
    auto self_id = catter::env::get_env_value(envp, config::hook::KEY_CATTER_COMMAND_ID);
    if(self_id == nullptr) {
        WARN("catter self id not found in environment");
        return session;
    } else {
        session.self_id = self_id;
    }
    auto ipc = catter::env::get_env_value(envp, config::hook::KEY_CATTER_IPC);
    if(ipc == nullptr) {
        WARN("catter ipc pipe not found in environment");
        return session;
    } else {
        session.ipc = ipc;
    }
    if(!session.is_valid()) {
        WARN("session is invalid");
        return session;
    }

    INFO("session from env: catter_proxy={}, self_id={}, ipc={}",
         session.proxy_path,
         session.self_id,
         session.ipc);
    return session;
}
}  // namespace catter
