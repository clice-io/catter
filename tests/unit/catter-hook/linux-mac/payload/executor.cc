#include "executor.h"
#include "opt-data/catter-proxy/parser.h"
#include "session.h"
#include "mock/mock_linker.h"
#include "mock/mock_resolver.h"

#include <zest/zest.h>

#include <filesystem>

namespace ct = catter;
namespace fs = std::filesystem;

namespace {

MockLinker linker;
MockResolver resolver;
ct::Session session{.proxy_path = "/usr/local/bin/catter-proxy", .self_id = "123"};
ct::Executor executor(linker, session, resolver);

char* const empty_envp[] = {nullptr};

TEST_SUITE(executor) {
    TEST_CASE(execve_success_flow) {
        linker.reset();
        resolver.add_file("/bin/ls", "/bin/ls");

        char* const argv[] = {(char*)"ls", (char*)"-la", nullptr};

        // Act
        int res = executor.execve("/bin/ls", argv, empty_envp);

        // Assert
        EXPECT_TRUE(res == 0);
        EXPECT_TRUE(linker.last_path ==
                    session.proxy_path);  // << "Should intercept and execute proxy";

        // Verify the proxy received the correct intercepted instructions
        auto f = [&]() {
            auto parse_res = catter::optdata::catter_proxy::parse_opt(linker.last_argv);
            EXPECT_TRUE(parse_res.parent_id == session.self_id);
            EXPECT_TRUE(parse_res.executable == "/bin/ls");

            auto& raw_args = parse_res.raw_argv;
            EXPECT_TRUE(raw_args.size() == 2);
            EXPECT_TRUE(raw_args[0] == "ls");
            EXPECT_TRUE(raw_args[1] == "-la");
        };
        EXPECT_NOTHROWS(f());
    };

    TEST_CASE(execvpe_success_using_mock_PATH_resolution) {
        linker.reset();
        resolver.add_file("python", "/usr/bin/python");

        char* const argv[] = {(char*)"python", (char*)"script.py", nullptr};

        int res = executor.execvpe("python", argv, empty_envp);

        EXPECT_TRUE(res == 0);
        EXPECT_TRUE(linker.last_path == session.proxy_path);
        // Verify translation of relative 'python' to absolute path
        auto f = [&]() {
            auto parse_res = catter::optdata::catter_proxy::parse_opt(linker.last_argv);
            EXPECT_TRUE(parse_res.executable == "/usr/bin/python");
        };
        EXPECT_NOTHROWS(f());
    };

    TEST_CASE(posix_spawn_success_flow) {
        linker.reset();
        resolver.add_file("/app/run", "/app/run");

        char* const argv[] = {(char*)"run", (char*)"--arg1", nullptr};
        pid_t pid;

        int res = executor.posix_spawn(&pid, "/app/run", nullptr, nullptr, argv, empty_envp);

        EXPECT_TRUE(res == 0);
        EXPECT_TRUE(linker.last_path == session.proxy_path);

        auto f = [&]() {
            auto parse_res = catter::optdata::catter_proxy::parse_opt(linker.last_argv);
            EXPECT_TRUE(parse_res.executable == "/app/run");
            EXPECT_TRUE(parse_res.raw_argv.at(1) == "--arg1");
        };
        EXPECT_NOTHROWS(f());
    };

    TEST_CASE(executor_handles_linker_failure) {
        linker.reset();
        resolver.add_file("/bin/true", "/bin/true");
        linker.should_fail = true;

        char* const argv[] = {(char*)"true", nullptr};
        int res = executor.execve("/bin/true", argv, empty_envp);

        EXPECT_TRUE(res == -1);
        EXPECT_TRUE(errno == ENOSYS);
    };
};
}  // namespace
