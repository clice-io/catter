#include <boost/ut.hpp>
#include <filesystem>
#include <format>
#include <fstream>
#include <system_error>
#include "resolver.h"
#include "temp_file_manager.h"

namespace ut = boost::ut;
namespace fs = std::filesystem;
namespace ct = catter;

ut::suite<"resolver"> resolver = [] {
    const auto resolver = ct::Resolver{};
    std::error_code ec;
    ct::TempFileManager manager("./tmp");
    ut::test("test current dir") = [&] {
        manager.create("./tmp1", ec);
        ut::expect(!ec);
        fs::path p = "./tmp/tmp1";
        auto res1 = resolver.from_current_directory(p.string());
        ut::expect(res1.has_value() && res1.value() == p);
        auto res2 = resolver.from_current_directory("./aaa");
        ut::expect(!res2.has_value());
    };
    ut::test("test from search path") = [&] {
        manager.create("./aaa", ec);
        ut::expect(!ec);
        fs::path p = "./tmp/aaa";
        auto res1 = resolver.from_search_path(
            "aaa",
            std::format("/usr/bin:{}", fs::absolute(manager.root).string()).c_str());
        ut::expect(res1.has_value() && res1.value() == fs::absolute(p));
        auto res2 = resolver.from_search_path(
            "aap",
            std::format("/usr/bin:{}", fs::absolute(manager.root).string()).c_str());
        ut::expect(!res2.has_value());
        auto res3 = resolver.from_search_path(
            p.string(),
            std::format("/usr/bin:{}", fs::absolute(manager.root).string()).c_str());
        ut::expect(res3.has_value() && res3.value() == p);
    };
    ut::test("test path") = [&] {
        manager.create("./bbb", ec);
        ut::expect(!ec);
        fs::path p = "./tmp/bbb";
        auto path = std::format("PATH={}", fs::absolute(manager.root).c_str());
        const char* envp[] = {path.c_str(), "ENV=X", 0};
        auto res1 = resolver.from_path("bbb", envp);
        ut::expect(res1.has_value() && res1.value() == fs::absolute(p));
        auto res2 = resolver.from_path("bb", envp);
        ut::expect(!res2.has_value());
    };
};
