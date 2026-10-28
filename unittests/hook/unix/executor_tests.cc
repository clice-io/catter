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

#include "hook/unix/executor.h"
#include "support/hook.h"
#include "support/messages.h"
#include "utils/temp_file_manager.h"

namespace ct = catter::hook;

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

catter::TempFileManager manager("catter-executor-");
CapturedCall exec_call;
CapturedCall spawn_call;

const std::string proxy = "/tmp/catter-proxy";

/// Sets variable `name` of this process's environment while it lives.
class ScopedEnv {
public:
    ScopedEnv(std::string name, std::string value) : name(std::move(name)) {
        if(auto* old = std::getenv(this->name.c_str()); old != nullptr) {
            previous = old;
        }
        ZEXPECT(::setenv(this->name.c_str(), value.c_str(), 1) == 0);
    }

    ~ScopedEnv() {
        if(previous.has_value()) {
            ::setenv(name.c_str(), previous->c_str(), 1);
            return;
        }
        ::unsetenv(name.c_str());
    }

private:
    std::string name;

    std::optional<std::string> previous;
};

/// Strings as a null-terminated array of mutable C strings, as argv and envp are.
class MutableCStrings {
public:
    MutableCStrings(std::initializer_list<std::string_view> strings) :
        strings(strings.begin(), strings.end()) {
        for(auto& string: this->strings) {
            pointers.push_back(string.data());
        }
        pointers.push_back(nullptr);
    }

    char* const* data() noexcept {
        return pointers.data();
    }

private:
    std::vector<std::string> strings;

    std::vector<char*> pointers;
};

/// The environment of a process the hook runs in, which names its session.
MutableCStrings session_env = {
    std::string(catter::env::PROXY) + "=" + proxy,
    std::string(catter::env::IPC) + "=/tmp/ipc-1.sock",
    std::string(catter::env::COMMAND_ID) + "=7",
};

/// The environment of a process that has no session.
MutableCStrings no_session_env = {};

std::vector<std::string> collect_values(char* const values[]) {
    std::vector<std::string> result;
    if(values == nullptr) {
        return result;
    }
    for(std::size_t i = 0; values[i] != nullptr; i += 1) {
        result.emplace_back(values[i]);
    }
    return result;
}

/// The value of variable `name` in `envp`; none when it has none.
std::optional<std::string> value_of(const std::vector<std::string>& envp, std::string_view name) {
    auto prefix = std::string(name) + "=";
    for(const auto& entry: envp) {
        if(entry.starts_with(prefix)) {
            return entry.substr(prefix.size());
        }
    }
    return std::nullopt;
}

int fake_execve(const char* path, char* const argv[], char* const envp[]) {
    exec_call.calls += 1;
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
    spawn_call.calls += 1;
    spawn_call.path = path == nullptr ? "" : path;
    spawn_call.argv = collect_values(argv);
    spawn_call.envp = collect_values(envp);
    return spawn_call.result;
}

/// A script `name` in the manager's directory, by its absolute path.
std::string create_executable(std::string_view name, std::string_view content = "#!/bin/sh\n") {
    ZEXPECT(!manager.create(name, content));
    return manager.path(name);
}

void expect_proxy_command(const CapturedCall& call,
                          std::string_view executable,
                          std::string_view argv0) {
    ZEXPECT(call.path == proxy);
    ZEXPECT(call.argv.size() >= 3);
    ZEXPECT(call.argv.at(0) == proxy);
    ZEXPECT(call.argv.at(1) == executable);
    ZEXPECT(call.argv.at(2) == argv0);
}

ZEST_SUITE(executor) {

ZEST_CASE(execve_starts_the_proxy_with_the_session_and_without_the_hook) {
    exec_call.reset(42);
    spawn_call.reset();

    auto executable = create_executable("execve-tool");
    MutableCStrings argv = {"execve-tool", "-c", "main.cc"};

    std::string command_id = std::string(catter::env::COMMAND_ID) + "=99";
    std::string proxy_path = std::string(catter::env::PROXY) + "=/tmp/ignored-proxy";
    std::string preload =
        std::string(catter::hook::PRELOAD) + "=/tmp/libkeep.so:/tmp/" + catter::hook::LIBRARY;
    MutableCStrings envp = {command_id, proxy_path, preload, "LANG=C"};

    ct::Executor executor;
    executor.init(session_env.data(), fake_execve, fake_posix_spawn);

    auto result = executor.execve(executable.c_str(), argv.data(), envp.data());

    ZEXPECT(result == 42);
    ZEXPECT(exec_call.calls == 1);
    expect_proxy_command(exec_call, executable, "execve-tool");
    ZEXPECT(exec_call.argv.at(3) == "-c");
    ZEXPECT(exec_call.argv.at(4) == "main.cc");
    ZEXPECT(value_of(exec_call.envp, catter::env::COMMAND_ID) == "7");
    ZEXPECT(value_of(exec_call.envp, catter::env::IPC) == "/tmp/ipc-1.sock");
    ZEXPECT(!value_of(exec_call.envp, catter::env::PROXY));
    ZEXPECT(value_of(exec_call.envp, "LANG") == "C");
    ZEXPECT(value_of(exec_call.envp, catter::hook::PRELOAD) == "/tmp/libkeep.so");
}

ZEST_CASE(execvp_resolves_with_the_process_environment) {
    exec_call.reset(51);

    auto executable = create_executable("path-tool");
    auto search_path = std::string(kota::fs::path::parent(executable));
    ScopedEnv path_env("PATH", search_path);
    ScopedEnv command_id(std::string(catter::env::COMMAND_ID), "11");

    MutableCStrings argv = {"path-tool"};

    ct::Executor executor;
    executor.init(session_env.data(), fake_execve, fake_posix_spawn);

    auto result = executor.execvp("path-tool", argv.data());

    ZEXPECT(result == 51);
    ZEXPECT(exec_call.calls == 1);
    expect_proxy_command(exec_call, executable, "path-tool");
    ZEXPECT(value_of(exec_call.envp, "PATH") == search_path);
    ZEXPECT(value_of(exec_call.envp, catter::env::COMMAND_ID) == "7");
}

ZEST_CASE(a_program_s_own_environment_names_the_session_to_the_proxy) {
    exec_call.reset(55);

    auto executable = create_executable("scrubbed-tool");
    MutableCStrings argv = {"scrubbed-tool"};
    MutableCStrings envp = {"PATH=/usr/bin"};

    ct::Executor executor;
    executor.init(session_env.data(), fake_execve, fake_posix_spawn);

    auto result = executor.execve(executable.c_str(), argv.data(), envp.data());

    ZEXPECT(result == 55);
    expect_proxy_command(exec_call, executable, "scrubbed-tool");
    ZEXPECT(value_of(exec_call.envp, catter::env::IPC) == "/tmp/ipc-1.sock");
    ZEXPECT(value_of(exec_call.envp, catter::env::COMMAND_ID) == "7");
    ZEXPECT(value_of(exec_call.envp, "PATH") == "/usr/bin");
}

ZEST_CASE(execvP_looks_in_the_search_path_it_is_given) {
    exec_call.reset(52);

    auto executable = create_executable("search-tool");
    auto search_path = "/definitely/missing:" + std::string(kota::fs::path::parent(executable));
    MutableCStrings argv = {"search-tool"};
    MutableCStrings envp = {"LANG=C"};

    ct::Executor executor;
    executor.init(session_env.data(), fake_execve, fake_posix_spawn);

    auto result = executor.execvP("search-tool", search_path.c_str(), argv.data(), envp.data());

    ZEXPECT(result == 52);
    ZEXPECT(exec_call.calls == 1);
    expect_proxy_command(exec_call, executable, "search-tool");
}

ZEST_CASE(execvP_fails_with_enoent_for_a_program_found_nowhere) {
    exec_call.reset();
    MutableCStrings argv = {"missing-tool"};

    ct::Executor executor;
    executor.init(session_env.data(), fake_execve, fake_posix_spawn);

    errno = 0;
    auto result = executor.execvP("missing-tool", "/definitely/missing", argv.data(), nullptr);

    ZEXPECT(result == -1);
    ZEXPECT(errno == ENOENT);
    ZEXPECT(exec_call.calls == 0);
}

ZEST_CASE(execve_without_a_proxy_runs_the_program_itself) {
    exec_call.reset(61);

    auto executable = create_executable("lone-tool");
    MutableCStrings argv = {"lone-tool", "-v"};
    MutableCStrings envp = {"LANG=C"};

    ct::Executor executor;
    executor.init(no_session_env.data(), fake_execve, fake_posix_spawn);

    auto result = executor.execve(executable.c_str(), argv.data(), envp.data());

    ZEXPECT(result == 61);
    ZEXPECT(exec_call.path == executable);
    ZEXPECT((exec_call.argv == std::vector<std::string>{"lone-tool", "-v"}));
    ZEXPECT(exec_call.envp == std::vector<std::string>{"LANG=C"});
}

ZEST_CASE(posix_spawn_without_a_proxy_starts_the_program_itself) {
    spawn_call.reset(62);

    auto executable = create_executable("lone-spawn-tool");
    MutableCStrings argv = {"lone-spawn-tool"};
    MutableCStrings envp = {"LANG=C"};

    ct::Executor executor;
    executor.init(no_session_env.data(), fake_execve, fake_posix_spawn);

    pid_t pid = 0;
    auto result =
        executor.posix_spawn(&pid, executable.c_str(), nullptr, nullptr, argv.data(), envp.data());

    ZEXPECT(result == 62);
    ZEXPECT(spawn_call.path == executable);
    ZEXPECT(spawn_call.argv == std::vector<std::string>{"lone-spawn-tool"});
    ZEXPECT(spawn_call.envp == std::vector<std::string>{"LANG=C"});
}

ZEST_CASE(execve_without_a_proxy_fails_for_a_missing_program) {
    exec_call.reset(60);

    MutableCStrings argv = {"missing-tool"};

    ct::Executor executor;
    executor.init(no_session_env.data(), fake_execve, fake_posix_spawn);

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
    executor.init(session_env.data(), fake_execve, fake_posix_spawn);

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
    executor.init(session_env.data(), fake_execve, fake_posix_spawn);

    pid_t pid = 0;
    auto result =
        executor.posix_spawn(&pid, executable.c_str(), nullptr, nullptr, argv.data(), nullptr);

    ZEXPECT(result == 17);
    ZEXPECT(spawn_call.calls == 1);
    expect_proxy_command(spawn_call, executable, "spawn-tool");
}

ZEST_CASE(spawn_boundary_returns_error_code) {
    spawn_call.reset();
    MutableCStrings argv = {"tool"};

    ct::Executor executor;
    executor.init(session_env.data(), fake_execve, fake_posix_spawn);

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
    executor.init(session_env.data(), fake_execve, fake_posix_spawn);

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
    executor.init(session_env.data(), fake_execve, fake_posix_spawn);

    auto result = executor.execvp("shell-script", argv.data());

    ZEXPECT(result == 53);
    expect_proxy_command(exec_call, executable, "shell-script");
}

ZEST_CASE(execvp_runs_a_script_that_names_no_interpreter_through_the_proxy) {
    exec_call.reset(54);

    auto executable = create_executable("empty-shebang", "#!\necho run by /bin/sh\n");
    ScopedEnv path_env("PATH", std::string(kota::fs::path::parent(executable)));
    MutableCStrings argv = {"empty-shebang"};

    ct::Executor executor;
    executor.init(session_env.data(), fake_execve, fake_posix_spawn);

    auto result = executor.execvp("empty-shebang", argv.data());

    ZEXPECT(result == 54);
    expect_proxy_command(exec_call, executable, "empty-shebang");
}

ZEST_CASE(posix_spawn_leaves_a_script_with_a_missing_interpreter_to_the_system) {
    spawn_call.reset(ENOENT);

    auto executable = create_executable("lost-interpreter", "#!/definitely/missing/sh\n");
    MutableCStrings argv = {"lost-interpreter"};

    ct::Executor executor;
    executor.init(session_env.data(), fake_execve, fake_posix_spawn);

    pid_t pid = 0;
    auto result =
        executor.posix_spawn(&pid, executable.c_str(), nullptr, nullptr, argv.data(), nullptr);

    ZEXPECT(result == ENOENT);
    ZEXPECT(spawn_call.path == executable);
    ZEXPECT(spawn_call.argv == std::vector<std::string>{"lost-interpreter"});
}

};  // ZEST_SUITE(executor)

}  // namespace
