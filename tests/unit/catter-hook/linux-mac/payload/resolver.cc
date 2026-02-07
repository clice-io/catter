#include "resolver.h"
#include "temp_file_manager.h"

#include <zest/zest.h>

#include <filesystem>
#include <format>
#include <system_error>

namespace fs = std::filesystem;
namespace ct = catter;

namespace {
const auto resolver = ct::Resolver{};
std::error_code ec;
ct::TempFileManager manager("./tmp");

TEST_SUITE(resolver) {

    TEST_CASE(test_current_dir) {
        manager.create("./tmp1", ec);
        EXPECT_TRUE(!ec);
        fs::path p = "./tmp/tmp1";
        auto res1 = resolver.from_current_directory(p.string());
        EXPECT_TRUE(res1.has_value() && res1.value() == p);
        auto res2 = resolver.from_current_directory("./aaa");
        EXPECT_TRUE(!res2.has_value());
    };
    TEST_CASE(test_from_search_path) {
        manager.create("./aaa", ec);
        EXPECT_TRUE(!ec);
        fs::path p = "./tmp/aaa";
        auto res1 = resolver.from_search_path(
            "aaa",
            std::format("/usr/bin:{}", fs::absolute(manager.root).string()).c_str());
        EXPECT_TRUE(res1.has_value() && res1.value() == fs::absolute(p));
        auto res2 = resolver.from_search_path(
            "aap",
            std::format("/usr/bin:{}", fs::absolute(manager.root).string()).c_str());
        EXPECT_TRUE(!res2.has_value());
        auto res3 = resolver.from_search_path(
            p.string(),
            std::format("/usr/bin:{}", fs::absolute(manager.root).string()).c_str());
        EXPECT_TRUE(res3.has_value() && res3.value() == p);
    };
    TEST_CASE(test_path) {
        manager.create("./bbb", ec);
        EXPECT_TRUE(!ec);
        fs::path p = "./tmp/bbb";
        auto path = std::format("PATH={}", fs::absolute(manager.root).c_str());
        const char* envp[] = {path.c_str(), "ENV=X", 0};
        auto res1 = resolver.from_path("bbb", envp);
        EXPECT_TRUE(res1.has_value() && res1.value() == fs::absolute(p));
        auto res2 = resolver.from_path("bb", envp);
        EXPECT_TRUE(!res2.has_value());
    };
};
}  // namespace
