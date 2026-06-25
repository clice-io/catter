#include "command.h"

#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>
#include <kota/zest/macro.h>
#include <kota/zest/zest.h>

#include "session.h"

namespace ct = catter;

namespace {
ct::Session session{.proxy_path = "/usr/local/bin/catter-proxy", .self_id = "99"};

TEST_SUITE(cmd_builder) {

TEST_CASE(proxy_cmd_constructs_correct_arguments) {
    std::filesystem::path target_path = "/usr/bin/gcc";

    std::vector<const char*> original_argv = {"gcc", "-c", "main.c", nullptr};

    auto cmd = ct::build_proxy_command(
        session,
        target_path,
        std::span<const char* const>{original_argv.data(), original_argv.size() - 1});

    // 1. Verify basic properties
    EXPECT_TRUE(cmd.path == session.proxy_path);

    // 2. Verify argv[0] convention
    EXPECT_TRUE(cmd.argv.at(0) == session.proxy_path);

    std::vector<std::string> expected_argv = {
        session.proxy_path,
        "-p",
        session.self_id,
        "--exec",
        target_path,
        "--",
        "gcc",
        "-c",
        "main.c",
    };
    EXPECT_TRUE(cmd.argv == expected_argv);
};

TEST_CASE(error_cmd_formats_message_correctly_without_separator) {
    std::filesystem::path target_path = "/usr/bin/invalid";

    std::vector<const char*> original_argv = {"invalid", "--help", nullptr};
    const char* error_msg = "File not found";

    auto cmd = ct::build_error_command(
        session,
        error_msg,
        target_path,
        std::span<const char* const>{original_argv.data(), original_argv.size() - 1});

    bool found_separator = false;
    for(const auto& arg: cmd.argv) {
        if(arg == "--")
            found_separator = true;
    }
    EXPECT_TRUE(!found_separator);

    std::string last_arg = cmd.argv.back();
    EXPECT_TRUE(last_arg.find("Catter Proxy Error: File not found") != std::string::npos);
    EXPECT_TRUE(last_arg.find("in command: invalid --help") != std::string::npos);

    EXPECT_TRUE(cmd.argv.size() == 6);
    EXPECT_TRUE(cmd.argv.at(0) == session.proxy_path);
    EXPECT_TRUE(cmd.argv.at(1) == "-p");
    EXPECT_TRUE(cmd.argv.at(2) == session.self_id);
    EXPECT_TRUE(cmd.argv.at(3) == "--exec");
    EXPECT_TRUE(cmd.argv.at(4) == target_path);
};
};  // TEST_SUITE(cmd_builder)
}  // namespace
