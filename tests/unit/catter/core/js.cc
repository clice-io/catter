#include "js.h"
#include "config/js-test.h"
#include "temp_file_manager.h"
#include "util/output.h"

#include <cstdio>
#include <eventide/zest/macro.h>
#include <eventide/zest/zest.h>

#include <exception>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;
using namespace catter;
using namespace catter::js;

namespace {

void ensure_qjs_initialized(const fs::path& js_path) {
    static bool initialized = false;
    if(!initialized) {
        js::init_qjs({.pwd = js_path});
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
    js::run_js_file(content, full_path.string());
}

void run_basic_js_case(std::string_view file_name, bool with_fs_test_env = false) {
    try {
        auto js_path = fs::path(config::data::js_test_path.data());
        ensure_qjs_initialized(js_path);

        if(with_fs_test_env) {
            auto js_path_res = fs::path(config::data::js_test_res_path.data());
            TempFileManager manager(js_path_res / "fs-test-env");

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
    } catch(qjs::Exception& ex) {
        output::redLn("{}", ex.what());
        throw ex;
    }
}

bool auto_js_case_uses_fs_test_env(const fs::path& relative_path) {
    return relative_path.filename() == "fs.js";
}

std::vector<fs::path> collect_auto_js_case_paths(const fs::path& js_path) {
    std::vector<fs::path> paths;
    const auto auto_path = js_path / "auto";
    if(!fs::exists(auto_path)) {
        return paths;
    }

    for(const auto& entry: fs::recursive_directory_iterator(auto_path)) {
        if(!entry.is_regular_file() || entry.path().extension() != ".js") {
            continue;
        }
        paths.push_back(entry.path().lexically_relative(js_path));
    }

    std::sort(paths.begin(), paths.end());
    return paths;
}

std::string auto_js_case_name(const fs::path& relative_path) {
    auto name = relative_path.lexically_relative("auto");
    name.replace_extension();
    return name.generic_string();
}

void run_auto_js_case(const fs::path& relative_path) {
    run_basic_js_case(relative_path.generic_string(), auto_js_case_uses_fs_test_env(relative_path));
}

eventide::zest::TestState run_auto_js_test_case(const fs::path& relative_path) {
    try {
        run_auto_js_case(relative_path);
        return eventide::zest::TestState::Passed;
    } catch(const std::exception& ex) {
        output::redLn("auto js test failed: {}: {}", relative_path.string(), ex.what());
        return eventide::zest::TestState::Failed;
    } catch(...) {
        output::redLn("auto js test failed: {}: unknown exception", relative_path.string());
        return eventide::zest::TestState::Fatal;
    }
}

std::vector<eventide::zest::TestCase> auto_js_test_cases() {
    std::vector<eventide::zest::TestCase> cases;
    const auto js_path = fs::path(config::data::js_test_path.data());

    for(const auto& relative_path: collect_auto_js_case_paths(js_path)) {
        const auto full_path = (js_path / relative_path).string();
        const auto case_name = auto_js_case_name(relative_path);
        cases.emplace_back(eventide::zest::TestCase{
            .name = case_name,
            .path = full_path,
            .line = 1,
            .attrs = {},
            .test = [relative_path] { return run_auto_js_test_case(relative_path); },
        });
    }

    return cases;
}

}  // namespace

TEST_SUITE(js_tests) {
TEST_CASE(run_service_js_file_and_callbacks) {
    auto f = [&]() {
        auto js_path = fs::path(config::data::js_test_path.data());
        ensure_qjs_initialized(js_path);
        run_js_file_by_name(js_path, "service.js");

        js::CatterRuntime runtime{
            .supportActions = {js::ActionType::skip,
                               js::ActionType::drop,
                               js::ActionType::abort,
                               js::ActionType::modify},
            .supportEvents = {js::EventType::finish, js::EventType::output},
            .type = js::CatterRuntime::Type::inject,
            .supportParentId = true,
        };

        js::CatterConfig config{
            .scriptPath = "script.ts",
            .scriptArgs = {"--input", "compile_commands.json"},
            .buildSystemCommand = {"xmake", "build"},
            .runtime = runtime,
            .options = {.log = true},
            .execute = true,
        };

        auto updated_config = js::on_start(config);
        EXPECT_TRUE(updated_config.scriptPath == config.scriptPath);
        EXPECT_TRUE(updated_config.scriptArgs.size() == 3);
        EXPECT_TRUE(updated_config.scriptArgs.back() == "--from-service");
        EXPECT_TRUE(updated_config.options.log == false);
        EXPECT_TRUE(updated_config.execute == false);

        js::CommandData data{
            .cwd = "/tmp",
            .exe = "clang++",
            .argv = {"clang++", "main.cc", "-c"},
            .env = {"CC=clang++", "CATTER_LOG=1"},
            .runtime = runtime,
            .parent = 41,
        };

        auto action = js::on_command(7, data);
        action.visit([&]<auto E>(const Tag<E>& tag) {
            if constexpr(E == js::ActionType::modify) {
                EXPECT_TRUE(tag.data.argv.size() == 4);
                EXPECT_TRUE(tag.data.argv.back() == "--from-service");
                EXPECT_TRUE(tag.data.parent.has_value());
                EXPECT_TRUE(tag.data.parent.value() == 41);
            } else {
                EXPECT_TRUE(E == js::ActionType::modify);
            }
        });

        js::CatterErr err{.msg = "spawn failed"};
        auto error_action = js::on_command(7, std::unexpected(err));
        EXPECT_TRUE(error_action.type() == js::ActionType::skip);

        js::ExecutionEvent output_event = js::Tag<js::EventType::output>{
            .stdOut = "hello from stdout",
            .stdErr = "hello from stderr",
            .code = 0,
        };
        js::on_execution(7, output_event);

        js::ExecutionEvent finish_event = js::Tag<js::EventType::finish>{
            .code = 0,
        };
        js::on_execution(7, finish_event);

        js::on_finish(finish_event);
    };

    EXPECT_NOTHROWS(f());
};

TEST_CASE(run_cdb_js_file) {
    auto f = [&]() {
        run_basic_js_case("cdb.js");
    };

    EXPECT_NOTHROWS(f());
};

TEST_CASE(run_js_file_reports_async_error_message_and_stack) {
    auto f = [&]() {
        auto js_path = fs::path(config::data::js_test_path.data());
        ensure_qjs_initialized(js_path);

        bool caught = false;
        try {
            js::run_js_file("await Promise.reject(new Error('async boom'));\n", "reject.js");
        } catch(const qjs::Exception& ex) {
            caught = true;
            std::string message = ex.what();
            EXPECT_TRUE(message.contains("async boom"));
            EXPECT_TRUE(message.contains("Stack Trace:"));
            EXPECT_TRUE(message.contains("reject.js"));
        }

        EXPECT_TRUE(caught);
    };

    EXPECT_NOTHROWS(f());
};
};  // TEST_SUITE(js_tests)

namespace {

const bool auto_js_tests_registered = [] {
    eventide::zest::Runner::instance().add_suite("js_auto_tests", &auto_js_test_cases);
    return true;
}();

}  // namespace
