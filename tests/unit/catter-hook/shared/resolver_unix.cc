#ifndef CATTER_WINDOWS

#include "shared/resolver.h"
#include "temp_file_manager.h"

#include <eventide/zest/zest.h>

#include <filesystem>
#include <format>
#include <system_error>

namespace fs = std::filesystem;
namespace ct = catter::hook::shared;

namespace {

std::error_code ec;
catter::TempFileManager manager("./tmp_shared");

TEST_SUITE(shared_unix_resolver) {

TEST_CASE(path_like_resolves_current_directory_entry) {
    manager.create("./tool", ec);
    EXPECT_TRUE(!ec);

    auto path = fs::path("./tmp_shared/tool");
    auto resolved = ct::resolve_path_like(path.string());
    EXPECT_TRUE(resolved.has_value());
    EXPECT_TRUE(*resolved == path);
}

TEST_CASE(search_path_resolves_plain_file_name) {
    manager.create("./runner", ec);
    EXPECT_TRUE(!ec);

    auto resolved = ct::resolve_from_search_path(
        "runner",
        std::format("/usr/bin:{}", fs::absolute(manager.root).string()));
    EXPECT_TRUE(resolved.has_value());
    EXPECT_TRUE(*resolved == fs::absolute(manager.root) / "runner");
}

TEST_CASE(environment_resolution_uses_PATH) {
    manager.create("./env-tool", ec);
    EXPECT_TRUE(!ec);

    auto path = std::format("PATH={}", fs::absolute(manager.root).string());
    const char* envp[] = {path.c_str(), nullptr};
    auto resolved = ct::resolve_from_environment("env-tool", envp);
    EXPECT_TRUE(resolved.has_value());
    EXPECT_TRUE(*resolved == fs::absolute(manager.root) / "env-tool");
}

TEST_CASE(path_like_resolution_fails_for_missing_entry) {
    auto resolved = ct::resolve_path_like("./definitely-missing");
    EXPECT_TRUE(!resolved.has_value());
}

};  // TEST_SUITE(shared_unix_resolver)

}  // namespace

#endif
