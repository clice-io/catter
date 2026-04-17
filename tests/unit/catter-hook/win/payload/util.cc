
#include "win/payload/util.h"

#include <kota/zest/zest.h>

#include <string>

#include <windows.h>

namespace ct = catter;

namespace {

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

TEST_SUITE(win_payload_util) {
TEST_CASE(get_proxy_path_reads_environment_variable) {
    ScopedEnvVar scope(L"CATTER_PROXY_PATH", L"C:\\tmp\\proxy.exe");
    EXPECT_TRUE(ct::win::payload::get_proxy_path<char>() == "C:\\tmp\\proxy.exe");
    EXPECT_TRUE(ct::win::payload::get_proxy_path<wchar_t>() == L"C:\\tmp\\proxy.exe");
};

TEST_CASE(get_ipc_id_reads_environment_variable) {
    ScopedEnvVar scope(L"CATTER_IPC_ID", L"12345");
    EXPECT_TRUE(ct::win::payload::get_ipc_id<char>() == "12345");
    EXPECT_TRUE(ct::win::payload::get_ipc_id<wchar_t>() == L"12345");
};
};  // TEST_SUITE(win_payload_util)

}  // namespace
