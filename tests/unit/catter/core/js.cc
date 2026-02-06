#include "js.h"
#include "config/js-test.h"
#include "temp_file_manager.h"

#include <zest/macro.h>
#include <zest/zest.h>

#include <filesystem>

namespace fs = std::filesystem;

TEST_SUITE(js) {
    TEST_CASE(run_js_files) {
        auto js_path = fs::path(catter::config::data::js_test_path.data());
        auto js_path_res = fs::path(catter::config::data::js_test_res_path.data());
        catter::TempFileManager manager(js_path_res / "fs-test-env");
        std::error_code ec;
        manager.create("a/tmp.txt", ec, "Alpha!\nBeta!\nKid A;\nend;");
        EXPECT_TRUE(!ec);
        manager.create("b/tmp2.txt", ec, "Ok computer!\n");
        EXPECT_TRUE(!ec);
        manager.create("c/a.txt", ec);
        EXPECT_TRUE(!ec);
        manager.create("c/b.txt", ec);
        EXPECT_TRUE(!ec);

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
            EXPECT_NOTHROWS(
                catter::core::js::run_js_file(content, js_file.path().string()));  //<< name;
        }
    };
};
