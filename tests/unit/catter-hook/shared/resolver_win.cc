#ifdef CATTER_WINDOWS

#include "shared/resolver.h"

#include <eventide/zest/zest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include <windows.h>

namespace fs = std::filesystem;
namespace ct = catter::hook::shared;

namespace {

struct TempSandbox {
    fs::path root;

    TempSandbox() {
        root = fs::temp_directory_path() /
               (L"catter_hook_shared_resolver_ut_" + std::to_wstring(GetCurrentProcessId()) + L"_" +
                std::to_wstring(GetTickCount64()));
        fs::create_directories(root);
    }

    ~TempSandbox() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
};

void touch_file(const fs::path& file) {
    fs::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::binary);
    out << "test";
}

bool same_existing_file(const fs::path& left, const fs::path& right) {
    std::error_code ec;
    return fs::equivalent(left, right, ec);
}

TEST_SUITE(shared_win_resolver) {

TEST_CASE(application_name_searches_current_directory) {
    TempSandbox sandbox;
    auto cwd = sandbox.root / "cwd";
    fs::create_directories(cwd);
    auto executable = cwd / "clang.exe";
    touch_file(executable);

    ct::WindowsResolver<char> resolver({
        .current_directory = cwd.string(),
        .current_drive_root = cwd.root_path().string(),
        .module_directory = {},
        .system_directory = {},
        .windows_directory = {},
        .path_entries = {},
    });

    auto resolved = resolver.resolve_application_name("clang");
    EXPECT_TRUE(resolved.has_value());
    EXPECT_TRUE(same_existing_file(fs::path(*resolved), executable));
}

TEST_CASE(command_line_token_prefers_module_directory) {
    TempSandbox sandbox;
    auto module_dir = sandbox.root / "module";
    auto path_dir = sandbox.root / "path";
    auto module_tool = module_dir / "tool.exe";
    auto path_tool = path_dir / "tool.exe";
    touch_file(module_tool);
    touch_file(path_tool);

    ct::WindowsResolver<char> resolver({
        .current_directory = {},
        .current_drive_root = {},
        .module_directory = module_dir.string(),
        .system_directory = {},
        .windows_directory = {},
        .path_entries = {path_dir.string()},
    });

    auto resolved = resolver.resolve_command_line_token("tool");
    EXPECT_TRUE(resolved.has_value());
    EXPECT_TRUE(same_existing_file(fs::path(*resolved), module_tool));
}

TEST_CASE(extract_command_line_token_handles_quoted_input) {
    EXPECT_TRUE(ct::extract_command_line_token<char>("  \"clang-cl\" /c test.cc") == "clang-cl");
}

TEST_CASE(command_line_token_reports_failure_for_missing_entry) {
    ct::WindowsResolver<char> resolver({
        .current_directory = {},
        .current_drive_root = {},
        .module_directory = {},
        .system_directory = {},
        .windows_directory = {},
        .path_entries = {},
    });

    auto resolved = resolver.resolve_command_line_token("definitely-missing-command");
    EXPECT_TRUE(!resolved.has_value());
}

};  // TEST_SUITE(shared_win_resolver)

}  // namespace

#endif
