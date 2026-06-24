#include "executor.h"

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <spawn.h>
#include <kota/zest/zest.h>

#include "temp_file_manager.h"
#include "unix/config.h"

namespace ct = catter;
namespace cfg = catter::config::hook;
namespace fs = std::filesystem;

namespace {

struct CapturedCall {
    int calls = 0;
    int result = 0;
    std::string path;
    std::vector<std::string> argv;
    std::vector<std::string> envp;

    void reset(int new_result = 0) {
        calls = 0;
        result = new_result;
        path.clear();
        argv.clear();
        envp.clear();
    }
};

ct::TempFileManager manager("./tmp-executor");
CapturedCall exec_call;
CapturedCall spawn_call;

ct::Session valid_session{.proxy_path = "/tmp/catter-proxy", .self_id = "7"};

class ScopedEnv {
public:
    ScopedEnv(std::string key, std::string value) : m_key(std::move(key)) {
        if(auto* old_value = std::getenv(m_key.c_str()); old_value != nullptr) {
            m_old_value = old_value;
        }
        EXPECT_TRUE(::setenv(m_key.c_str(), value.c_str(), 1) == 0);
    }

    ~ScopedEnv() {
        if(m_old_value.has_value()) {
            ::setenv(m_key.c_str(), m_old_value->c_str(), 1);
            return;
        }
        ::unsetenv(m_key.c_str());
    }

private:
    std::string m_key;
    std::optional<std::string> m_old_value;
};

class MutableCStrings {
public:
    MutableCStrings(std::initializer_list<std::string_view> values) {
        m_values.reserve(values.size());
        m_ptrs.reserve(values.size() + 1);
        for(auto value: values) {
            m_values.emplace_back(value);
        }
        for(auto& value: m_values) {
            m_ptrs.push_back(value.data());
        }
        m_ptrs.push_back(nullptr);
    }

    char* const* data() noexcept {
        return m_ptrs.data();
    }

private:
    std::vector<std::string> m_values;
    std::vector<char*> m_ptrs;
};

std::vector<std::string> collect_values(char* const values[]) {
    std::vector<std::string> result;
    if(values == nullptr) {
        return result;
    }
    for(std::size_t i = 0; values[i] != nullptr; ++i) {
        result.emplace_back(values[i]);
    }
    return result;
}

bool has_env_entry(const std::vector<std::string>& envp, std::string_view key) {
    auto prefix = std::string(key) + "=";
    for(const auto& entry: envp) {
        if(entry.starts_with(prefix)) {
            return true;
        }
    }
    return false;
}

int fake_execve(const char* path, char* const argv[], char* const envp[]) {
    ++exec_call.calls;
    exec_call.path = path == nullptr ? "" : path;
    exec_call.argv = collect_values(argv);
    exec_call.envp = collect_values(envp);
    return exec_call.result;
}

int fake_posix_spawn(pid_t*,
                     const char* path,
                     const posix_spawn_file_actions_t*,
                     const posix_spawnattr_t*,
                     char* const argv[],
                     char* const envp[]) {
    ++spawn_call.calls;
    spawn_call.path = path == nullptr ? "" : path;
    spawn_call.argv = collect_values(argv);
    spawn_call.envp = collect_values(envp);
    return spawn_call.result;
}

fs::path create_executable(std::string_view name) {
    std::error_code ec;
    manager.create(std::string(name), ec);
    EXPECT_TRUE(!ec);
    return manager.root / name;
}

void expect_proxy_command(const CapturedCall& call,
                          const ct::Session& session,
                          const fs::path& executable,
                          std::string_view argv0) {
    EXPECT_TRUE(call.path == session.proxy_path);
    EXPECT_TRUE(call.argv.size() >= 7);
    EXPECT_TRUE(call.argv.at(0) == session.proxy_path);
    EXPECT_TRUE(call.argv.at(1) == "-p");
    EXPECT_TRUE(call.argv.at(2) == session.self_id);
    EXPECT_TRUE(call.argv.at(3) == "--exec");
    EXPECT_TRUE(call.argv.at(4) == executable.string());
    EXPECT_TRUE(call.argv.at(5) == "--");
    EXPECT_TRUE(call.argv.at(6) == argv0);
}

TEST_SUITE(executor) {

TEST_CASE(execve_builds_proxy_command_with_sanitized_environment) {
    exec_call.reset(42);
    spawn_call.reset();

    auto executable = create_executable("execve-tool");
    MutableCStrings argv = {"execve-tool", "-c", "main.cc"};

    std::string command_id = std::string(cfg::KEY_CATTER_COMMAND_ID) + "=99";
    std::string proxy_path = std::string(cfg::KEY_CATTER_PROXY_PATH) + "=/tmp/ignored-proxy";
    std::string preload =
        std::string(cfg::KEY_PRELOAD) + "=/tmp/libkeep.so:/tmp/" + cfg::HOOK_LIB_NAME;
    std::string lang = "LANG=C";
    MutableCStrings envp = {command_id, proxy_path, preload, lang};

    ct::Executor executor;
    executor.init(valid_session, fake_execve, fake_posix_spawn);

    auto result = executor.execve(executable.c_str(), argv.data(), envp.data());

    EXPECT_TRUE(result == 42);
    EXPECT_TRUE(exec_call.calls == 1);
    expect_proxy_command(exec_call, valid_session, executable, "execve-tool");
    EXPECT_TRUE(exec_call.argv.at(7) == "-c");
    EXPECT_TRUE(exec_call.argv.at(8) == "main.cc");
    EXPECT_TRUE(!has_env_entry(exec_call.envp, cfg::KEY_CATTER_COMMAND_ID));
    EXPECT_TRUE(!has_env_entry(exec_call.envp, cfg::KEY_CATTER_PROXY_PATH));
    EXPECT_TRUE(has_env_entry(exec_call.envp, "LANG"));
    EXPECT_TRUE(has_env_entry(exec_call.envp, cfg::KEY_PRELOAD));
    EXPECT_TRUE(exec_call.envp.at(0) == std::string(cfg::KEY_PRELOAD) + "=/tmp/libkeep.so" ||
                exec_call.envp.at(1) == std::string(cfg::KEY_PRELOAD) + "=/tmp/libkeep.so");
}

TEST_CASE(execvp_resolves_with_process_environment_before_sanitizing) {
    exec_call.reset(51);

    auto executable = create_executable("path-tool");
    auto search_path = fs::absolute(manager.root).string();
    ScopedEnv path_env("PATH", search_path);
    ScopedEnv command_id(std::string(cfg::KEY_CATTER_COMMAND_ID), "11");

    MutableCStrings argv = {"path-tool"};

    ct::Executor executor;
    executor.init(valid_session, fake_execve, fake_posix_spawn);

    auto result = executor.execvp("path-tool", argv.data());

    EXPECT_TRUE(result == 51);
    EXPECT_TRUE(exec_call.calls == 1);
    expect_proxy_command(exec_call, valid_session, fs::absolute(executable), "path-tool");
    EXPECT_TRUE(has_env_entry(exec_call.envp, "PATH"));
    EXPECT_TRUE(!has_env_entry(exec_call.envp, cfg::KEY_CATTER_COMMAND_ID));
}

TEST_CASE(execve_invalid_session_still_fails_before_fallback_when_target_is_missing) {
    exec_call.reset(60);

    ct::Session invalid_session{};
    MutableCStrings argv = {"missing-tool"};

    ct::Executor executor;
    executor.init(invalid_session, fake_execve, fake_posix_spawn);

    errno = 0;
    auto result = executor.execve("/definitely/missing/tool", argv.data(), nullptr);

    EXPECT_TRUE(result == -1);
    EXPECT_TRUE(errno == ENOENT);
    EXPECT_TRUE(exec_call.calls == 0);
}

TEST_CASE(exec_boundary_maps_payload_errors_to_errno) {
    exec_call.reset();
    MutableCStrings argv = {"tool"};

    ct::Executor executor;
    executor.init(valid_session, fake_execve, fake_posix_spawn);

    errno = 0;
    auto result = executor.execve(nullptr, argv.data(), nullptr);

    EXPECT_TRUE(result == -1);
    EXPECT_TRUE(errno == EFAULT);
    EXPECT_TRUE(exec_call.calls == 0);
}

TEST_CASE(posix_spawn_builds_proxy_command_and_returns_spawn_result) {
    spawn_call.reset(17);

    auto executable = create_executable("spawn-tool");
    MutableCStrings argv = {"spawn-tool"};

    ct::Executor executor;
    executor.init(valid_session, fake_execve, fake_posix_spawn);

    pid_t pid = 0;
    auto result =
        executor.posix_spawn(&pid, executable.c_str(), nullptr, nullptr, argv.data(), nullptr);

    EXPECT_TRUE(result == 17);
    EXPECT_TRUE(spawn_call.calls == 1);
    expect_proxy_command(spawn_call, valid_session, executable, "spawn-tool");
}

TEST_CASE(spawn_boundary_returns_error_code) {
    spawn_call.reset();
    MutableCStrings argv = {"tool"};

    ct::Executor executor;
    executor.init(valid_session, fake_execve, fake_posix_spawn);

    pid_t pid = 0;
    errno = 0;
    auto result = executor.posix_spawn(&pid, nullptr, nullptr, nullptr, argv.data(), nullptr);

    EXPECT_TRUE(result == EFAULT);
    EXPECT_TRUE(errno == EFAULT);
    EXPECT_TRUE(spawn_call.calls == 0);
}

};  // TEST_SUITE(executor)

}  // namespace
