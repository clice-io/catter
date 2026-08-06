#include <exception>
#include <filesystem>
#include <utility>
#include <kota/zest/macro.h>
#include <kota/zest/zest.h>
#include <kota/async/io/loop.h>

#include "js_case.h"
#include "js/js.h"

namespace fs = std::filesystem;

namespace {

kota::task<> run_service_js_callbacks(fs::path js_path) {
    catter::js::RuntimeScope runtime_scope;

    std::exception_ptr error;
    try {
        runtime_scope.start({.pwd = js_path});
        co_await catter::js::run_script(
            catter::tests::js::load_js_file_by_name(js_path, "service.js"),
            (js_path / "service.js").string());

        catter::js::CatterRuntime runtime{
            .supportActions = {catter::js::ActionType::skip,
                               catter::js::ActionType::drop,
                               catter::js::ActionType::abort,
                               catter::js::ActionType::modify},
            .type = catter::js::CatterRuntime::Type::inject,
            .supportParentId = true,
        };

        catter::js::CatterConfig config{
            .scriptPath = "script.ts",
            .scriptArgs = {"--input",   "compile_commands.json"                                   },
            .buildSystemCommand = {"xmake",     "build"                                                   },
            .runtime = runtime,
            .options = {.log = true, .stdioMode = catter::js::CatterOptions::StdioMode::inherit},
            .execute = true,
        };

        auto updated_config = co_await catter::js::on_start(config);
        EXPECT_TRUE(updated_config.scriptPath == config.scriptPath);
        EXPECT_TRUE(updated_config.scriptArgs.size() == 3);
        EXPECT_TRUE(updated_config.scriptArgs.back() == "--from-service");
        EXPECT_TRUE(updated_config.options.log == false);
        EXPECT_TRUE(updated_config.options.stdioMode ==
                    catter::js::CatterOptions::StdioMode::capture);
        EXPECT_TRUE(updated_config.execute == true);

        catter::js::CommandData data{
            .cwd = "/tmp",
            .exe = "clang++",
            .argv = {"clang++", "main.cc", "-c"},
            .env = {"CC=clang++", "CATTER_LOG=1"},
            .runtime = runtime,
            .parent = 41,
        };

        auto action = co_await catter::js::on_command(7, data);
        action.visit([&]<auto E>(const catter::js::Tag<E>& tag) {
            if constexpr(E == catter::js::ActionType::modify) {
                EXPECT_TRUE(tag.data.argv.size() == 4);
                EXPECT_TRUE(tag.data.argv.back() == "--from-service");
                EXPECT_TRUE(tag.data.parent.has_value());
                EXPECT_TRUE(tag.data.parent.value() == 41);
            } else {
                EXPECT_TRUE(E == catter::js::ActionType::modify);
            }
        });

        catter::js::CatterErr err{.msg = "spawn failed"};
        auto error_action = co_await catter::js::on_command(7, std::unexpected(err));
        EXPECT_TRUE(error_action.type() == catter::js::ActionType::skip);

        catter::js::ProcessResult execution_result{
            .code = 0,
            .stdOut = "hello from stdout",
            .stdErr = "hello from stderr",
        };
        co_await catter::js::on_execution(7, execution_result);

        catter::js::ProcessResult finish_result{
            .code = 0,
        };
        co_await catter::js::on_finish(finish_result);
    } catch(...) {
        error = std::current_exception();
    }

    co_await runtime_scope.stop();

    if(error) {
        std::rethrow_exception(error);
    }
    co_return;
}

}  // namespace

TEST_SUITE(js_file_tests) {
TEST_CASE(run_service_js_file_and_callbacks) {
    auto f = [&]() {
        auto task = run_service_js_callbacks(catter::tests::js::js_test_root());

        kota::event_loop loop;
        loop.schedule(task);
        loop.run();
        task.result();
    };

    EXPECT_NOTHROWS(f());
};

};  // TEST_SUITE(js_file_tests)
