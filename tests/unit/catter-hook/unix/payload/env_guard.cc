#include "env_guard.h"
#include "environment.h"
#include "unix/config.h"

#include <zest/zest.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace ct = catter;
namespace cfg = catter::config::hook;

namespace {

const char* find_entry(const char** envp, std::string_view key) {
    for(std::size_t i = 0; envp[i] != nullptr; ++i) {
        if(ct::env::is_entry_of(envp[i], key)) {
            return envp[i];
        }
    }
    return nullptr;
}

TEST_SUITE(env_guard) {
    TEST_CASE(removes_injected_keys_and_filters_hook_preload) {
        std::string keep_lib_1 = "/tmp/libkeep-1.so";
        std::string keep_lib_2 = "/tmp/libkeep-2.so";
        std::string hook_lib = "/tmp/";
        hook_lib += cfg::RELATIVE_PATH_OF_HOOK_LIB;

        std::string command_id = std::string(cfg::KEY_CATTER_COMMAND_ID) + "=42";
        std::string proxy_path = std::string(cfg::KEY_CATTER_PROXY_PATH) + "=/tmp/catter-proxy";
        std::string preload =
            std::string(cfg::KEY_PRELOAD) + "=" + keep_lib_1 + ":" + hook_lib + ":" + keep_lib_2;
        std::string lang = "LANG=en_US.UTF-8";

        const char* raw_env[] = {command_id.data(),
                                 proxy_path.data(),
                                 preload.data(),
                                 lang.data(),
                                 nullptr};
        const char** envp = raw_env;

        ct::EnvGuard guard(&envp);

        EXPECT_TRUE(find_entry(envp, cfg::KEY_CATTER_COMMAND_ID) == nullptr);
        EXPECT_TRUE(find_entry(envp, cfg::KEY_CATTER_PROXY_PATH) == nullptr);

        auto cleaned_preload = find_entry(envp, cfg::KEY_PRELOAD);
        EXPECT_TRUE(cleaned_preload != nullptr);
        std::string expected_preload =
            std::string(cfg::KEY_PRELOAD) + "=" + keep_lib_1 + ":" + keep_lib_2;
        EXPECT_TRUE(std::string_view(cleaned_preload) == expected_preload);

        EXPECT_TRUE(find_entry(envp, "LANG") != nullptr);
    };

    TEST_CASE(preload_becomes_empty_when_only_hook_entry_exists) {
        std::string hook_lib = "/tmp/";
        hook_lib += cfg::RELATIVE_PATH_OF_HOOK_LIB;
        std::string preload = std::string(cfg::KEY_PRELOAD) + "=" + hook_lib;

        const char* raw_env[] = {preload.data(), nullptr};
        const char** envp = raw_env;

        ct::EnvGuard guard(&envp);

        auto cleaned_preload = find_entry(envp, cfg::KEY_PRELOAD);
        EXPECT_TRUE(cleaned_preload == nullptr);
    };
};

}  // namespace
