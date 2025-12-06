#include "js.h"
#include <boost/ut.hpp>
#include <filesystem>
#include <print>
#include <filesystem>
#include "libconfig/js-test.h"
#include "libutil/output.h"
namespace ut = boost::ut;

namespace fs = std::filesystem;

#define JS_PATH #JS_DIR

ut::suite<"js_tests"> js_io_tests = [] {
    auto js_path = fs::path(catter::config::data::js_test_path);
    catter::core::js::init_qjs();
    ut::test("js file test") = [&] {
        for(const auto& js_file: fs::directory_iterator{js_path}) {
            if(js_file.path().extension() == ".js") {
                std::string content;
                {
                    std::ifstream ifs(js_file.path());
                    content = std::string((std::istreambuf_iterator<char>(ifs)),
                                          std::istreambuf_iterator<char>());
                }
                catter::output::blueLn("Running JS test file: {}",
                                       js_file.path().filename().string());
                catter::core::js::run_js_file(content, js_file.path().filename().string());
            }
        }
    };
};
