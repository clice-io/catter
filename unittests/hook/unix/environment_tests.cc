#include <string>
#include <string_view>
#include <kota/zest/zest.h>

#include "hook/unix/environment.h"
#include "support/hook.h"
#include "support/messages.h"

namespace ct = catter::hook;

namespace {

std::string entry(std::string_view name, std::string_view value) {
    return std::string(name) + "=" + std::string(value);
}

const char* value_of(ct::Environment& env, std::string_view name) {
    return ct::get_env_value(env.entries.data(), name);
}

ZEST_SUITE(environment) {
ZEST_CASE(catter_proxy_gets_the_session_without_the_hook) {
    const std::string hook = std::string("/opt/catter/") + catter::hook::LIBRARY;
    auto ipc = entry(catter::env::IPC, "/tmp/ipc-0.sock");
    auto command_id = entry(catter::env::COMMAND_ID, "42");
    auto preload = entry(catter::hook::PRELOAD, "/tmp/libkeep-1.so:" + hook + ":/tmp/libkeep-2.so");
    std::string lang = "LANG=en_US.UTF-8";
    char* envp[] = {ipc.data(), command_id.data(), preload.data(), lang.data(), nullptr};
    std::string session[] = {entry(catter::env::IPC, "/tmp/ipc-1.sock"),
                             entry(catter::env::COMMAND_ID, "7")};

    auto env = ct::proxy_environment(envp, session);

    ZEXPECT(std::string_view(value_of(env, catter::env::IPC)) == "/tmp/ipc-1.sock");
    ZEXPECT(std::string_view(value_of(env, catter::env::COMMAND_ID)) == "7");
    ZEXPECT(std::string_view(value_of(env, catter::hook::PRELOAD)) ==
            "/tmp/libkeep-1.so:/tmp/libkeep-2.so");
    ZEXPECT(std::string_view(value_of(env, "LANG")) == "en_US.UTF-8");
};

ZEST_CASE(a_preload_of_the_hook_alone_goes) {
    auto preload = entry(catter::hook::PRELOAD, std::string("/tmp/") + catter::hook::LIBRARY);
    char* envp[] = {preload.data(), nullptr};

    auto env = ct::proxy_environment(envp, {});

    ZEXPECT(value_of(env, catter::hook::PRELOAD) == nullptr);
};

#ifdef CATTER_LINUX
ZEST_CASE(spaces_separate_preloaded_libraries_as_colons_do) {
    auto preload =
        entry(catter::hook::PRELOAD, std::string("/tmp/libkeep.so /tmp/") + catter::hook::LIBRARY);
    char* envp[] = {preload.data(), nullptr};

    auto env = ct::proxy_environment(envp, {});

    ZEXPECT(std::string_view(value_of(env, catter::hook::PRELOAD)) == "/tmp/libkeep.so");
};
#endif
};  // ZEST_SUITE(environment)

}  // namespace
