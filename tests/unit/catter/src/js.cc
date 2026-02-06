
#include "js.h"
#include "config/js-test.h"
#include "util/output.h"

#include <zest/zest.h>

#include <cstdio>
#include <exception>
#include <filesystem>
#include <format>
#include <filesystem>
#include <string>
#include <string_view>
#include <fstream>


namespace fs = std::filesystem;

TEST_SUITE(js_tests) {

TEST_CASE(load_and_run_js_tests_from_directory) {
    auto js_path = fs::path(catter::config::data::js_test_path.data());
    catter::core::js::init_qjs({.pwd = js_path});
    for(const auto& js_file: fs::directory_iterator{js_path}) {
        if(js_file.path().extension() != ".js") {
            continue;
        }
        auto name = std::format("js test: file {}", js_file.path().string());

        std::string content;
        std::ifstream ifs{js_file.path()};

        content = std::string((std::istreambuf_iterator<char>(ifs)),
                                std::istreambuf_iterator<char>());
        EXPECT_NOTHROWS(catter::core::js::run_js_file(content, js_file.path().string()));
    }
    
}
};
