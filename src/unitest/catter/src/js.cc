#include <boost/ut.hpp>
#include "js.h"

#include "js.h"
#include <boost/ut.hpp>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <format>
#include <print>
#include <filesystem>
#include "config/js-test.h"
#include "qjs.h"
#include "util/output.h"
namespace ut = boost::ut;

namespace fs = std::filesystem;
using namespace boost::ut::literals;
namespace ut = boost::ut;

ut::suite<"js"> js = [] {
    auto js_path = fs::path(catter::config::data::js_test_path.data());
    catter::core::js::init_qjs({.pwd = js_path});
    for(const auto& js_file: fs::directory_iterator{js_path}) {
        if(js_file.path().extension() != ".js") {
            continue;
        }
        ut::test(std::format("js test: file {}", js_file.path().string())) = [&] {
            std::string content;
            {
                std::ifstream ifs(js_file.path());
                content = std::string((std::istreambuf_iterator<char>(ifs)),
                                      std::istreambuf_iterator<char>());
            }
            try {
                catter::core::js::run_js_file(content, js_file.path().string());
            } catch(std::exception& e) {
                catter::output::redLn("\n{}", e.what());
                ut::expect(false);
            }
        };
    }
};
