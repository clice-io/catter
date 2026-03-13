#include "js.h"
#include "config/js-test.h"
#include "temp_file_manager.h"

#include <eventide/zest/macro.h>
#include <eventide/zest/zest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {

void ensure_qjs_initialized(const fs::path& js_path) {
    static bool initialized = false;
    if(!initialized) {
        catter::js::init_qjs({.pwd = js_path});
        initialized = true;
    }
}

void run_js_file_by_name(const fs::path& js_path, std::string_view file_name) {
    auto full_path = js_path / file_name;

    std::ifstream ifs{full_path};
    if(!ifs.good()) {
        throw std::runtime_error("js test file cannot be opened: " + full_path.string());
    }

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    catter::js::run_js_file(content, full_path.string());
}

void run_basic_js_case(std::string_view file_name, bool with_fs_test_env = false) {
    auto js_path = fs::path(catter::config::data::js_test_path.data());
    ensure_qjs_initialized(js_path);

    if(with_fs_test_env) {
        auto js_path_res = fs::path(catter::config::data::js_test_res_path.data());
        catter::TempFileManager manager(js_path_res / "fs-test-env");

        std::error_code ec;
        manager.create("a/tmp.txt", ec, "Alpha!\nBeta!\nKid A;\nend;");
        if(ec) {
            throw std::runtime_error("failed to prepare fs test file: a/tmp.txt");
        }
        manager.create("b/tmp2.txt", ec, "Ok computer!\n");
        if(ec) {
            throw std::runtime_error("failed to prepare fs test file: b/tmp2.txt");
        }
        manager.create("c/a.txt", ec);
        if(ec) {
            throw std::runtime_error("failed to prepare fs test file: c/a.txt");
        }
        manager.create("c/b.txt", ec);
        if(ec) {
            throw std::runtime_error("failed to prepare fs test file: c/b.txt");
        }

        run_js_file_by_name(js_path, file_name);
        return;
    }

    run_js_file_by_name(js_path, file_name);
}

}  // namespace

TEST_SUITE(js_tests) {
    TEST_CASE(run_fs_js_file) {
        auto f = [&]() {
            run_basic_js_case("fs.js", true);
        };

        EXPECT_NOTHROWS(f());
    };

    TEST_CASE(run_io_js_file) {
        auto f = [&]() {
            run_basic_js_case("io.js");
        };

        EXPECT_NOTHROWS(f());
    };

    TEST_CASE(run_os_js_file) {
        auto f = [&]() {
            run_basic_js_case("os.js");
        };

        EXPECT_NOTHROWS(f());
    };

    TEST_CASE(run_service_js_file_and_callbacks) {
        auto f = [&]() {
            auto js_path = fs::path(catter::config::data::js_test_path.data());
            ensure_qjs_initialized(js_path);
            run_js_file_by_name(js_path, "service.js");

            catter::js::CatterRuntime runtime{
                .supportActions = {catter::js::ActionType::skip,
                                   catter::js::ActionType::drop,
                                   catter::js::ActionType::abort,
                                   catter::js::ActionType::modify},
                .supportEvents = {catter::js::EventType::finish, catter::js::EventType::output},
                .type = catter::js::CatterRuntime::Type::inject,
                .supportParentId = true,
            };

            catter::js::CatterConfig config{
                .scriptPath = "script.ts",
                .scriptArgs = {"--input", "compile_commands.json"},
                .buildSystemCommand = {"xmake", "build"},
                .runtime = runtime,
                .options = {.log = true},
                .isScriptSupported = true,
            };

            auto updated_config = catter::js::on_start(config);
            EXPECT_TRUE(updated_config.scriptPath == config.scriptPath);
            EXPECT_TRUE(updated_config.scriptArgs.size() == 3);
            EXPECT_TRUE(updated_config.scriptArgs.back() == "--from-service");
            EXPECT_TRUE(updated_config.options.log == false);
            EXPECT_TRUE(updated_config.isScriptSupported == false);

            catter::js::CommandData data{
                .cwd = "/tmp",
                .exe = "clang++",
                .argv = {"clang++", "main.cc", "-c"},
                .env = {"CC=clang++", "CATTER_LOG=1"},
                .runtime = runtime,
                .parent = 41,
            };

            auto action = catter::js::on_command(7, data);
            EXPECT_TRUE(action.type == catter::js::ActionType::modify);
            EXPECT_TRUE(action.data.has_value());
            EXPECT_TRUE(action.data->argv.size() == 4);
            EXPECT_TRUE(action.data->argv.back() == "--from-service");
            EXPECT_TRUE(action.data->parent.has_value());
            EXPECT_TRUE(action.data->parent.value() == 41);

            catter::js::CatterErr err{.msg = "spawn failed"};
            auto error_action = catter::js::on_command(7, err);
            EXPECT_TRUE(error_action.type == catter::js::ActionType::skip);
            EXPECT_TRUE(!error_action.data.has_value());

            catter::js::ExecutionEvent output_event{
                .stdOut = std::string{"hello from stdout"},
                .stdErr = std::string{"hello from stderr"},
                .code = 0,
                .type = catter::js::EventType::output,
            };
            catter::js::on_execution(7, output_event);

            catter::js::ExecutionEvent finish_event{
                .stdOut = std::nullopt,
                .stdErr = std::nullopt,
                .code = 0,
                .type = catter::js::EventType::finish,
            };
            catter::js::on_execution(7, finish_event);

            catter::js::on_finish();
        };

        EXPECT_NOTHROWS(f());
    };
};
