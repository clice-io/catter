#include <algorithm>
#include <exception>
#include <filesystem>
#include <string>
#include <vector>
#include <kota/zest/zest.h>

#include "js_case.h"
#include "util/output.h"

namespace fs = std::filesystem;

namespace {

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
    catter::tests::js::run_basic_js_case(relative_path.generic_string(),
                                         auto_js_case_uses_fs_test_env(relative_path));
}

void run_auto_js_test_case(const fs::path& relative_path) {
    try {
        run_auto_js_case(relative_path);
    } catch(const std::exception& ex) {
        catter::output::redln("auto js test failed: {}: {}", relative_path.string(), ex.what());
        kota::zest::failure();
    } catch(...) {
        catter::output::redln("auto js test failed: {}: unknown exception", relative_path.string());
        kota::zest::failure();
    }
}

std::vector<kota::zest::TestCase> auto_js_test_cases() {
    std::vector<kota::zest::TestCase> cases;
    const auto js_path = catter::tests::js::js_test_root();

    for(const auto& relative_path: collect_auto_js_case_paths(js_path)) {
        const auto full_path = (js_path / relative_path).string();
        const auto case_name = auto_js_case_name(relative_path);
        cases.emplace_back(kota::zest::TestCase{
            .name = case_name,
            .path = full_path,
            .line = 1,
            .attrs = {},
            .test = [relative_path] { run_auto_js_test_case(relative_path); },
        });
    }

    return cases;
}

const bool auto_js_tests_registered = [] {
    kota::zest::Runner::instance().add_suite("js_auto_tests", &auto_js_test_cases);
    return true;
}();

}  // namespace
