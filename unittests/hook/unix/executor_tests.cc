#include <cerrno>
#include <cstdlib>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <spawn.h>
#include <kota/zest/zest.h>
#include <kota/fs/path.h>
#include <kota/fs/resolve.h>

#include "hook/unix/executor.h"
#include "support/config/hook_unix.h"
#include "utils/temp_file_manager.h"

namespace ct = catter::hook;
namespace cfg = catter::config;

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

catter::TempFileManager manager("./tmp-executor");
CapturedCall exec_call;
CapturedCall spawn_call;

ct::Session valid_session{.proxy_path = "/tmp/catter-proxy",
                          .self_id = "7",
                          .ipc = "/tmp/ipc-1.sock"};

class ScopedEnv {
public:
    ScopedEnv(std::string key, std::string value) : m_key(std::move(key)) {
        if(auto* old_value = std::getenv(m_key.c_str()); old_value != nullptr) {
            m_old_value = old_value;
        }
        ZEXPECT(::setenv(m_key.c_str(), value.c_str(), 1) == 0);
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

/// A script `name` in the manager's directory, by its absolute path.
std::string create_executable(std::string_view name, std::string_view content = "#!/bin/sh\n") {
    ZEXPECT(!manager.create(name, content));
    std::string path;
    ZEXPECT(!kota::fs::absolute(path, manager.path(name)));
    return path;
}

void expect_proxy_command(const CapturedCall& call,
                          const ct::Session& session,
                          std::string_view executable,
                          std::string_view argv0) {
    ZEXPECT(call.path == session.proxy_path);
    ZEXPECT(call.argv.size() >= 9);
    ZEXPECT(call.argv.at(0) == session.proxy_path);
    ZEXPECT(call.argv.at(1) == "-p");
    ZEXPECT(call.argv.at(2) == session.self_id);
    ZEXPECT(call.argv.at(3) == "--exec");
    ZEXPECT(call.argv.at(4) == executable);
    ZEXPECT(call.argv.at(5) == "--ipc");
    ZEXPECT(call.argv.at(6) == session.ipc);
    ZEXPECT(call.argv.at(7) == "--");
    ZEXPECT(call.argv.at(8) == argv0);
}

ZEST_SUITE(executor) {
// Every worker process removes ./tmp-executor when it exits, so these must run alone.
ZEST_SUITE_ATTRS(serial = true);

ZEST_CASE(execve_builds_proxy_command_with_sanitized_environment) {
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

    ZEXPECT(result == 42);
    ZEXPECT(exec_call.calls == 1);
    expect_proxy_command(exec_call, valid_session, executable, "execve-tool");
    ZEXPECT(exec_call.argv.at(9) == "-c");
    ZEXPECT(exec_call.argv.at(10) == "main.cc");
    ZEXPECT(!has_env_entry(exec_call.envp, cfg::KEY_CATTER_COMMAND_ID));
    ZEXPECT(!has_env_entry(exec_call.envp, cfg::KEY_CATTER_PROXY_PATH));
    ZEXPECT(has_env_entry(exec_call.envp, "LANG"));
    ZEXPECT(has_env_entry(exec_call.envp, cfg::KEY_PRELOAD));
    ZEXPECT((exec_call.envp.at(0) == std::string(cfg::KEY_PRELOAD) + "=/tmp/libkeep.so" ||
             exec_call.envp.at(1) == std::string(cfg::KEY_PRELOAD) + "=/tmp/libkeep.so"));
}

ZEST_CASE(execvp_resolves_with_process_environment_before_sanitizing) {
    exec_call.reset(51);

    auto executable = create_executable("path-tool");
    auto search_path = std::string(kota::fs::path::parent(executable));
    ScopedEnv path_env("PATH", search_path);
    ScopedEnv command_id(std::string(cfg::KEY_CATTER_COMMAND_ID), "11");

    MutableCStrings argv = {"path-tool"};

    ct::Executor executor;
    executor.init(valid_session, fake_execve, fake_posix_spawn);

    auto result = executor.execvp("path-tool", argv.data());

    ZEXPECT(result == 51);
    ZEXPECT(exec_call.calls == 1);
    expect_proxy_command(exec_call, valid_session, executable, "path-tool");
    ZEXPECT(has_env_entry(exec_call.envp, "PATH"));
    ZEXPECT(!has_env_entry(exec_call.envp, cfg::KEY_CATTER_COMMAND_ID));
}

ZEST_CASE(execvP_looks_in_the_search_path_it_is_given) {
    exec_call.reset(52);

    auto executable = create_executable("search-tool");
    auto search_path = "/definitely/missing:" + std::string(kota::fs::path::parent(executable));
    MutableCStrings argv = {"search-tool"};
    MutableCStrings envp = {"LANG=C"};

    ct::Executor executor;
    executor.init(valid_session, fake_execve, fake_posix_spawn);

    auto result = executor.execvP("search-tool", search_path.c_str(), argv.data(), envp.data());

    ZEXPECT(result == 52);
    ZEXPECT(exec_call.calls == 1);
    expect_proxy_command(exec_call, valid_session, executable, "search-tool");
}

ZEST_CASE(execvP_fails_with_enoent_for_a_program_found_nowhere) {
    exec_call.reset();
    MutableCStrings argv = {"missing-tool"};

    ct::Executor executor;
    executor.init(valid_session, fake_execve, fake_posix_spawn);

    errno = 0;
    auto result = executor.execvP("missing-tool", "/definitely/missing", argv.data(), nullptr);

    ZEXPECT(result == -1);
    ZEXPECT(errno == ENOENT);
    ZEXPECT(exec_call.calls == 0);
}

ZEST_CASE(execve_invalid_session_still_fails_before_fallback_when_target_is_missing) {
    exec_call.reset(60);

    ct::Session invalid_session{};
    MutableCStrings argv = {"missing-tool"};

    ct::Executor executor;
    executor.init(invalid_session, fake_execve, fake_posix_spawn);

    errno = 0;
    auto result = executor.execve("/definitely/missing/tool", argv.data(), nullptr);

    ZEXPECT(result == -1);
    ZEXPECT(errno == ENOENT);
    ZEXPECT(exec_call.calls == 0);
}

ZEST_CASE(exec_boundary_maps_payload_errors_to_errno) {
    exec_call.reset();
    MutableCStrings argv = {"tool"};

    ct::Executor executor;
    executor.init(valid_session, fake_execve, fake_posix_spawn);

    errno = 0;
    auto result = executor.execve(nullptr, argv.data(), nullptr);

    ZEXPECT(result == -1);
    ZEXPECT(errno == EFAULT);
    ZEXPECT(exec_call.calls == 0);
}

ZEST_CASE(posix_spawn_builds_proxy_command_and_returns_spawn_result) {
    spawn_call.reset(17);

    auto executable = create_executable("spawn-tool");
    MutableCStrings argv = {"spawn-tool"};

    ct::Executor executor;
    executor.init(valid_session, fake_execve, fake_posix_spawn);

    pid_t pid = 0;
    auto result =
        executor.posix_spawn(&pid, executable.c_str(), nullptr, nullptr, argv.data(), nullptr);

    ZEXPECT(result == 17);
    ZEXPECT(spawn_call.calls == 1);
    expect_proxy_command(spawn_call, valid_session, executable, "spawn-tool");
}

ZEST_CASE(spawn_boundary_returns_error_code) {
    spawn_call.reset();
    MutableCStrings argv = {"tool"};

    ct::Executor executor;
    executor.init(valid_session, fake_execve, fake_posix_spawn);

    pid_t pid = 0;
    errno = 0;
    auto result = executor.posix_spawn(&pid, nullptr, nullptr, nullptr, argv.data(), nullptr);

    ZEXPECT(result == EFAULT);
    ZEXPECT(errno == EFAULT);
    ZEXPECT(spawn_call.calls == 0);
}

ZEST_CASE(execve_leaves_a_file_of_no_executable_format_to_the_system) {
    exec_call.reset(-1);

    auto executable = create_executable("no-format", "echo not a program\n");
    MutableCStrings argv = {"no-format"};
    MutableCStrings envp = {"LANG=C"};

    ct::Executor executor;
    executor.init(valid_session, fake_execve, fake_posix_spawn);

    auto result = executor.execve(executable.c_str(), argv.data(), envp.data());

    ZEXPECT(result == -1);
    ZEXPECT(exec_call.calls == 1);
    ZEXPECT(exec_call.path == executable);
    ZEXPECT(exec_call.argv == std::vector<std::string>{"no-format"});
}

ZEST_CASE(execvp_runs_a_file_of_no_executable_format_through_the_proxy) {
    exec_call.reset(53);

    auto executable = create_executable("shell-script", "echo run by /bin/sh\n");
    ScopedEnv path_env("PATH", std::string(kota::fs::path::parent(executable)));
    MutableCStrings argv = {"shell-script"};

    ct::Executor executor;
    executor.init(valid_session, fake_execve, fake_posix_spawn);

    auto result = executor.execvp("shell-script", argv.data());

    ZEXPECT(result == 53);
    expect_proxy_command(exec_call, valid_session, executable, "shell-script");
}

ZEST_CASE(execvp_runs_a_script_that_names_no_interpreter_through_the_proxy) {
    exec_call.reset(54);

    auto executable = create_executable("empty-shebang", "#!\necho run by /bin/sh\n");
    ScopedEnv path_env("PATH", std::string(kota::fs::path::parent(executable)));
    MutableCStrings argv = {"empty-shebang"};

    ct::Executor executor;
    executor.init(valid_session, fake_execve, fake_posix_spawn);

    auto result = executor.execvp("empty-shebang", argv.data());

    ZEXPECT(result == 54);
    expect_proxy_command(exec_call, valid_session, executable, "empty-shebang");
}

ZEST_CASE(posix_spawn_leaves_a_script_with_a_missing_interpreter_to_the_system) {
    spawn_call.reset(ENOENT);

    auto executable = create_executable("lost-interpreter", "#!/definitely/missing/sh\n");
    MutableCStrings argv = {"lost-interpreter"};

    ct::Executor executor;
    executor.init(valid_session, fake_execve, fake_posix_spawn);

    pid_t pid = 0;
    auto result =
        executor.posix_spawn(&pid, executable.c_str(), nullptr, nullptr, argv.data(), nullptr);

    ZEXPECT(result == ENOENT);
    ZEXPECT(spawn_call.path == executable);
    ZEXPECT(spawn_call.argv == std::vector<std::string>{"lost-interpreter"});
}

};  // ZEST_SUITE(executor)

}  // namespace
