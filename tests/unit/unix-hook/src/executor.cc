
#include <boost/ut.hpp>
#include <filesystem>
#include "executor.h"
#include "opt-data/catter-proxy/parser.h"
#include "session.h"
#include "mock/mock_linker.h"
#include "mock/mock_resolver.h"

namespace ut = boost::ut;
namespace ct = catter;
namespace fs = std::filesystem;

ut::suite<"Executor"> executor_test = [] {
    MockLinker linker;
    MockResolver resolver;
    ct::Session session;

    session.proxy_path = "/opt/catter/bin/proxy";
    session.self_id = "123";

    ct::Executor executor(linker, session, resolver);

    using namespace ut;

    char* const empty_envp[] = {nullptr};

    test("execve success flow") = [&] {
        linker.reset();
        resolver.add_file("/bin/ls", "/bin/ls");

        char* const argv[] = {(char*)"ls", (char*)"-la", nullptr};

        // Act
        int res = executor.execve("/bin/ls", argv, empty_envp);

        // Assert
        expect(res == 0);
        expect(linker.last_path == session.proxy_path) << "Should intercept and execute proxy";

        // Verify the proxy received the correct intercepted instructions
        auto parse_res = catter::optdata::catter_proxy::parse_opt(linker.last_argv);

        expect(parse_res.has_value()) << "Proxy args should be parseable";
        if(parse_res.has_value()) {
            expect(parse_res->parent_id == session.self_id);
            expect(parse_res->executable == fs::path("/bin/ls"));

            expect(parse_res->raw_argv_or_err.has_value());
            if(parse_res->raw_argv_or_err.has_value()) {
                auto& raw_args = parse_res->raw_argv_or_err.value();
                expect(raw_args.size() == 2);
                expect(raw_args[0] == "ls");
                expect(raw_args[1] == "-la");
            }
        }
    };

    test("execvpe success using mock PATH resolution") = [&] {
        linker.reset();
        resolver.add_file("python", "/usr/bin/python");

        char* const argv[] = {(char*)"python", (char*)"script.py", nullptr};

        int res = executor.execvpe("python", argv, empty_envp);

        expect(res == 0);
        expect(linker.last_path == session.proxy_path);

        // Verify translation of relative 'python' to absolute path

        auto parse_res = catter::optdata::catter_proxy::parse_opt(linker.last_argv);
        expect(parse_res.has_value());
        if(parse_res.has_value()) {
            expect(parse_res->executable == fs::path("/usr/bin/python"));
        }
    };

    test("posix_spawn success flow") = [&] {
        linker.reset();
        resolver.add_file("/app/run", "/app/run");

        char* const argv[] = {(char*)"run", (char*)"--arg1", nullptr};
        pid_t pid;

        int res = executor.posix_spawn(&pid, "/app/run", nullptr, nullptr, argv, empty_envp);

        expect(res == 0);
        expect(linker.last_path == session.proxy_path);

        auto parse_res = catter::optdata::catter_proxy::parse_opt(linker.last_argv);
        expect(parse_res.has_value());
        if(parse_res.has_value()) {
            expect(parse_res->executable == fs::path("/app/run"));
            expect(parse_res->raw_argv_or_err.has_value());
            expect(parse_res->raw_argv_or_err->at(1) == "--arg1");
        }
    };

    test("executor handles linker failure") = [&] {
        linker.reset();
        resolver.add_file("/bin/true", "/bin/true");
        linker.should_fail = true;

        char* const argv[] = {(char*)"true", nullptr};
        int res = executor.execve("/bin/true", argv, empty_envp);

        expect(res == -1);
        expect(errno == ENOSYS);
    };
};
