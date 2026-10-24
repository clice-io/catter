#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace catter::tests::js {

std::filesystem::path js_test_root();
std::filesystem::path js_test_res_root();

std::string load_js_file_by_name(const std::filesystem::path& js_path, std::string_view file_name);
void run_async_js_case(std::string source, std::string file_name);
void run_basic_js_case(std::string_view file_name, bool with_fs_test_env = false);

}  // namespace catter::tests::js
