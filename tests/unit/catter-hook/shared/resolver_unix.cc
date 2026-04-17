#ifndef CATTER_WINDOWS

#include <filesystem>
#include <format>
#include <system_error>

#include "temp_file_manager.h"
#include "shared/resolver.h"

#include <kota/zest/zest.h>

namespace fs = std::filesystem;
namespace resolver = catter::hook::shared::resolver;

namespace {

std::error_code ec;
catter::TempFileManager manager("./tmp");

TEST_SUITE(shared_unix_resolver) {

TEST_CASE(resolve_path_like_supports_explicit_paths) {
    manager.create("./tool", ec);
    EXPECT_TRUE(!ec);

    auto resolved = resolver::resolve_path_like("./tmp/tool");
    EXPECT_TRUE(resolved.has_value() && resolved.value() == fs::path("./tmp/tool"));
};

TEST_CASE(resolve_from_search_path_supports_search_semantics) {
    manager.create("./runner", ec);
    EXPECT_TRUE(!ec);

    auto search_path = std::format("/usr/bin:{}", fs::absolute(manager.root).string());
    auto resolved = resolver::resolve_from_search_path("runner", search_path.c_str());
    EXPECT_TRUE(resolved.has_value());
    EXPECT_TRUE(resolved.value() == fs::absolute(manager.root / "runner"));
};

TEST_CASE(resolve_from_path_env_uses_environment_PATH) {
    manager.create("./path-tool", ec);
    EXPECT_TRUE(!ec);

    auto path = fs::absolute(manager.root).string();
    auto resolved = resolver::resolve_from_path_env("path-tool", path.c_str());
    EXPECT_TRUE(resolved.has_value());
    EXPECT_TRUE(resolved.value() == fs::absolute(manager.root / "path-tool"));
};

TEST_CASE(resolve_from_search_path_rejects_missing_entries) {
    auto resolved = resolver::resolve_from_search_path("missing-tool", "/usr/bin");
    EXPECT_TRUE(!resolved.has_value());
};

};  // TEST_SUITE(shared_unix_resolver)

}  // namespace

#endif
