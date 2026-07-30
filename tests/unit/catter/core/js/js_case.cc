#include "js_case.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <cpptrace/exceptions.hpp>
#include <kota/async/io/loop.h>

#include "temp_file_manager.h"
#include "js/js.h"
#include "util/output.h"

namespace fs = std::filesystem;

namespace catter::tests::js {
namespace {

constexpr std::string_view js_test_path = JS_TEST_PATH;
constexpr std::string_view js_test_res_path = JS_TEST_RES_PATH;

struct ScriptRunConfig {
    std::string script_content;
    std::string script_path;
    fs::path working_directory;
};

kota::task<> async_run(ScriptRunConfig config) {
    catter::js::RuntimeScope runtime;

    std::exception_ptr error;
    try {
        runtime.start({.pwd = std::move(config.working_directory)});
        co_await catter::js::run_script(config.script_content, config.script_path);
    } catch(...) {
        error = std::current_exception();
    }

    co_await runtime.stop();

    if(error) {
        std::rethrow_exception(error);
    }
    co_return;
}

void prepare_fs_test_env(TempFileManager& manager) {
    std::error_code ec;
    manager.create("a/tmp.txt", ec, "Alpha!\nBeta!\nKid A;\nend;");
    if(ec) {
        throw cpptrace::runtime_error("failed to prepare fs test file: a/tmp.txt");
    }
    manager.create("b/tmp2.txt", ec, "Ok computer!\n");
    if(ec) {
        throw cpptrace::runtime_error("failed to prepare fs test file: b/tmp2.txt");
    }
    manager.create("c/a.txt", ec);
    if(ec) {
        throw cpptrace::runtime_error("failed to prepare fs test file: c/a.txt");
    }
    manager.create("c/b.txt", ec);
    if(ec) {
        throw cpptrace::runtime_error("failed to prepare fs test file: c/b.txt");
    }
}

}  // namespace

fs::path js_test_root() {
    return fs::path(js_test_path);
}

fs::path js_test_res_root() {
    return fs::path(js_test_res_path);
}

std::string load_js_file_by_name(const fs::path& js_path, std::string_view file_name) {
    auto full_path = js_path / file_name;

    std::ifstream ifs{full_path};
    if(!ifs.good()) {
        throw cpptrace::runtime_error("js test file cannot be opened: " + full_path.string());
    }

    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

void run_async_js_case(std::string source, std::string file_name) {
    auto task = async_run(ScriptRunConfig{
        .script_content = std::move(source),
        .script_path = std::move(file_name),
        .working_directory = js_test_root(),
    });
    kota::event_loop loop;
    loop.schedule(task);
    loop.run();
    task.result();
}

void run_basic_js_case(std::string_view file_name, bool with_fs_test_env) {
    auto js_path = js_test_root();
    auto full_path = js_path / file_name;
    auto source = load_js_file_by_name(js_path, file_name);

    if(with_fs_test_env) {
        TempFileManager manager(js_test_res_root() / "fs-test-env");
        prepare_fs_test_env(manager);
        run_async_js_case(std::move(source), full_path.string());
        return;
    }

    run_async_js_case(std::move(source), full_path.string());
}

}  // namespace catter::tests::js
