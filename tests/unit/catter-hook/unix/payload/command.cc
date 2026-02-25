#include "command.h"
#include "session.h"

#include "opt/proxy/option.h"

#include <eventide/deco/runtime.h>
#include <eventide/zest/macro.h>
#include <eventide/zest/zest.h>

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>
#include <span>

namespace ct = catter;

namespace {
ct::Session session{.proxy_path = "/usr/local/bin/catter-proxy", .self_id = "99"};
ct::CmdBuilder builder(session);

TEST_SUITE(cmd_builder) {

    TEST_CASE(proxy_cmd_constructs_correct_arguments) {
        std::filesystem::path target_path = "/usr/bin/gcc";

        std::vector<char*> original_argv = {const_cast<char*>("gcc"),
                                            const_cast<char*>("-c"),
                                            const_cast<char*>("main.c"),
                                            nullptr};

        auto cmd = builder.proxy_cmd(
            target_path,
            std::span<char* const>{original_argv.data(), original_argv.size() - 1});

        // 1. Verify basic properties
        EXPECT_TRUE(cmd.path == session.proxy_path);

        // 2. Verify argv[0] convention
        EXPECT_TRUE(cmd.argv.at(0) == session.proxy_path);

        auto f = [&]() {
            auto parse_res = deco::cli::parse<catter::proxy::ProxyOption>(cmd.argv)->options;
            EXPECT_TRUE(*parse_res.parent_id == 99);
            EXPECT_TRUE(*parse_res.exec == target_path);
            EXPECT_TRUE(parse_res.args.value.has_value());
            auto& args = *parse_res.args;
            EXPECT_TRUE(args.size() == 3);
            EXPECT_TRUE(args.at(0) == "gcc");
            EXPECT_TRUE(args.at(2) == "main.c");
        };
        EXPECT_NOTHROWS(f());
    };

    TEST_CASE(error_cmd_formats_message_correctly_without_separator) {
        std::filesystem::path target_path = "/usr/bin/invalid";

        std::vector<char*> original_argv = {const_cast<char*>("invalid"),
                                            const_cast<char*>("--help"),
                                            nullptr};
        const char* error_msg = "File not found";

        auto cmd = builder.error_cmd(
            error_msg,
            target_path,
            std::span<char* const>{original_argv.data(), original_argv.size() - 1});

        bool found_separator = false;
        for(const auto& arg: cmd.argv) {
            if(arg == "--")
                found_separator = true;
        }
        EXPECT_TRUE(!found_separator);

        std::string last_arg = cmd.argv.back();
        EXPECT_TRUE(last_arg.find("Catter Proxy Error: File not found") != std::string::npos);
        EXPECT_TRUE(last_arg.find("in command: invalid --help") != std::string::npos);

        auto f = [&]() {
            auto parse_res = deco::cli::parse<catter::proxy::ProxyOption>(cmd.argv);
            EXPECT_FALSE(parse_res->options.args.value.has_value());
        };

        EXPECT_NOTHROWS(f());
    };
};
}  // namespace
