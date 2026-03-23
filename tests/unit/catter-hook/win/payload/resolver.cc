#include "win/payload/resolver.h"
#include "win/payload/util.h"

#include <eventide/zest/zest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include <windows.h>

namespace ct = catter;
namespace fs = std::filesystem;

namespace {

struct TempSandbox {
    fs::path root;

    TempSandbox() {
        root = fs::temp_directory_path() /
               (L"catter_win_payload_resolver_ut_" + std::to_wstring(GetCurrentProcessId()) + L"_" +
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

struct ScopedCurrentDirectory {
    std::wstring original;

    explicit ScopedCurrentDirectory(const fs::path& path) {
        wchar_t buffer[MAX_PATH];
        auto len = GetCurrentDirectoryW(MAX_PATH, buffer);
        if(len > 0) {
            original.assign(buffer, len);
        }
        SetCurrentDirectoryW(path.wstring().c_str());
    }

    ~ScopedCurrentDirectory() {
        if(!original.empty()) {
            SetCurrentDirectoryW(original.c_str());
        }
    }
};

struct ScopedEnvVar {
    std::wstring name;
    std::wstring original;
    bool had_original = false;

    ScopedEnvVar(std::wstring name_value, const std::wstring& value) : name(std::move(name_value)) {
        wchar_t buffer[32768];
        auto len = GetEnvironmentVariableW(name.c_str(), buffer, 32768);
        if(len > 0 && len < 32768) {
            had_original = true;
            original.assign(buffer, len);
        }
        SetEnvironmentVariableW(name.c_str(), value.c_str());
    }

    ~ScopedEnvVar() {
        if(had_original) {
            SetEnvironmentVariableW(name.c_str(), original.c_str());
        } else {
            SetEnvironmentVariableW(name.c_str(), nullptr);
        }
    }
};

TEST_SUITE(win_payload_resolver) {
    TEST_CASE(app_name_resolver_appends_exe_and_searches_current_directory) {
        TempSandbox sandbox;
        ScopedCurrentDirectory scope(sandbox.root);

        auto app_path = sandbox.root / "clang.exe";
        touch_file(app_path);

        auto resolved = ct::win::payload::create_app_name_resolver<char>().resolve("clang");
        EXPECT_TRUE(same_existing_file(fs::path(resolved), app_path));
    };

    TEST_CASE(app_name_resolver_does_not_append_exe_when_input_has_path) {
        TempSandbox sandbox;
        ScopedCurrentDirectory scope(sandbox.root);

        auto app_path = sandbox.root / "bin" / "lld";
        touch_file(app_path);

        auto resolved = ct::win::payload::create_app_name_resolver<char>().resolve("bin\\lld");
        EXPECT_TRUE(same_existing_file(fs::path(resolved), app_path));
        EXPECT_TRUE(fs::path(resolved).filename() == "lld");
    };

    TEST_CASE(command_line_resolver_searches_path_variable) {
        TempSandbox sandbox;
        auto cwd = sandbox.root / "cwd";
        fs::create_directories(cwd);
        ScopedCurrentDirectory scope(cwd);

        auto app_dir = sandbox.root / "pathbin";
        auto app_path = app_dir / "runner.exe";
        touch_file(app_path);

        ScopedEnvVar path_scope(L"PATH", app_dir.wstring());

        auto resolved = ct::win::payload::create_command_line_resolver<char>().resolve("runner");
        EXPECT_TRUE(same_existing_file(fs::path(resolved), app_path));
    };

    TEST_CASE(resolve_abspath_supports_quoted_command_line) {
        TempSandbox sandbox;
        ScopedCurrentDirectory scope(sandbox.root);

        auto app_path = sandbox.root / "quoted.exe";
        touch_file(app_path);

        auto resolved = ct::win::payload::resolve_abspath<char>(nullptr, "\"quoted\" --help");
        EXPECT_TRUE(same_existing_file(fs::path(resolved), app_path));
    };
};

}  // namespace
