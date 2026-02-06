
#include "util.h"
#include "util/output.h"
#include "opt-data/catter-proxy/parser.h"

#include <zest/zest.h>

#include <string_view>
#include <vector>

using namespace catter;
using namespace std::literals::string_view_literals;

TEST_SUITE(opt_catter_proxy) {
    TEST_CASE(option_table_has_expected_options) {
        auto argv = std::vector<std::string>{"-p", "1234"};
        optdata::catter_proxy::catter_proxy_opt_table.parse_args(
            argv,
            [&](std::expected<opt::ParsedArgument, std::string> arg) {
                EXPECT_TRUE(arg.has_value());
                if(arg.has_value()) {
                    EXPECT_TRUE(arg->option_id.id() == optdata::catter_proxy::OPT_PARENT_ID);
                    EXPECT_TRUE(arg->values.size() == 1);
                    EXPECT_TRUE(arg->values[0] == "1234");
                    EXPECT_TRUE(arg->get_spelling_view() == "-p");
                    EXPECT_TRUE(arg->index == 0);
                }
            });
        argv = split2vec("--exec /bin/ls");
        optdata::catter_proxy::catter_proxy_opt_table.parse_args(
            argv,
            [&](std::expected<opt::ParsedArgument, std::string> arg) {
                EXPECT_TRUE(arg.has_value());
                EXPECT_TRUE(arg->option_id.id() == optdata::catter_proxy::OPT_EXEC);
                EXPECT_TRUE(arg->values.size() == 1);
                EXPECT_TRUE(arg->values[0] == "/bin/ls");
                EXPECT_TRUE(arg->get_spelling_view() == "--exec");
                EXPECT_TRUE(arg->index == 0);
            });

        argv = split2vec("-p 12 --exec /usr/bin/clang++ -- clang++ --version");
        optdata::catter_proxy::catter_proxy_opt_table.parse_args(
            argv,
            [&](std::expected<opt::ParsedArgument, std::string> arg) {
                EXPECT_TRUE(arg.has_value());
                if(arg->option_id.id() == optdata::catter_proxy::OPT_PARENT_ID) {
                    EXPECT_TRUE(arg->values.size() == 1);
                    EXPECT_TRUE(arg->values[0] == "12");
                    EXPECT_TRUE(arg->get_spelling_view() == "-p");
                    EXPECT_TRUE(arg->index == 0);
                } else if(arg->option_id.id() == optdata::catter_proxy::OPT_EXEC) {
                    EXPECT_TRUE(arg->values.size() == 1);
                    EXPECT_TRUE(arg->values[0] == "/usr/bin/clang++");
                    EXPECT_TRUE(arg->get_spelling_view() == "--exec");
                    EXPECT_TRUE(arg->index == 2);
                } else {
                    EXPECT_TRUE(arg->option_id.id() == optdata::catter_proxy::OPT_INPUT);
                    EXPECT_TRUE(arg->get_spelling_view() == "--");
                    EXPECT_TRUE(arg->values.size() == 2);
                    EXPECT_TRUE(arg->index == 4);
                }
            });
    };
    TEST_CASE(test_unknown_option) {
        auto argv = split2vec("--unknown-option value");
        optdata::catter_proxy::catter_proxy_opt_table.parse_args(
            argv,
            [&](std::expected<opt::ParsedArgument, std::string> arg) {
                EXPECT_TRUE(arg.has_value());
                if(arg->option_id.id() == optdata::catter_proxy::OPT_UNKNOWN) {
                    EXPECT_TRUE(arg->option_id.id() == optdata::catter_proxy::OPT_UNKNOWN);
                    EXPECT_TRUE(arg->get_spelling_view() == "--unknown-option");
                    EXPECT_TRUE(arg->values.size() == 0);
                    EXPECT_TRUE(arg->index == 0);
                } else {
                    EXPECT_TRUE(arg->option_id.id() == optdata::catter_proxy::OPT_INPUT);
                    EXPECT_TRUE(arg->get_spelling_view() == "value");
                    EXPECT_TRUE(arg->values.size() == 0);
                    EXPECT_TRUE(arg->index == 1);
                }
            });
    };
    TEST_CASE(test_missing_value) {
        auto argv = split2vec("-p");
        optdata::catter_proxy::catter_proxy_opt_table.parse_args(
            argv,
            [&](std::expected<opt::ParsedArgument, std::string> arg) {
                EXPECT_TRUE(!arg.has_value());
                catter::output::blueLn("Test Error: {}", arg.error());
            });
    };
};

TEST_SUITE(opt_catter_proxy_parser) {
    TEST_CASE(parse_opt_success) {
        char* const argv[] =
            {(char*)"", (char*)"-p", (char*)"5678", (char*)"--exec", (char*)"/bin/echo", nullptr};
        auto res = optdata::catter_proxy::parse_opt(argv);
        EXPECT_TRUE(res.has_value());
        if(res.has_value()) {
            EXPECT_TRUE(res->parent_id == "5678");
            EXPECT_TRUE(res->executable == std::filesystem::path("/bin/echo"));
            EXPECT_TRUE(res->raw_argv_or_err.has_value());
            if(res->raw_argv_or_err.has_value()) {
                EXPECT_TRUE(res->raw_argv_or_err->size() == 0);
            }
        }
    };
    TEST_CASE(parse_opt_with_input_args) {
        char* const argv[] = {(char*)"",
                              (char*)"-p",
                              (char*)"91011",
                              (char*)"--exec",
                              (char*)"/usr/bin/python3",
                              (char*)"--",
                              (char*)"script.py",
                              (char*)"--verbose",
                              nullptr};
        auto res = optdata::catter_proxy::parse_opt(argv);
        EXPECT_TRUE(res.has_value());
        if(res.has_value()) {
            EXPECT_TRUE(res->parent_id == "91011");
            EXPECT_TRUE(res->executable == std::filesystem::path("/usr/bin/python3"));
            EXPECT_TRUE(res->raw_argv_or_err.has_value());
            if(res->raw_argv_or_err.has_value()) {
                EXPECT_TRUE(res->raw_argv_or_err->size() == 2);
                EXPECT_TRUE(res->raw_argv_or_err->at(0) == "script.py");
                EXPECT_TRUE(res->raw_argv_or_err->at(1) == "--verbose");
            }
        }
    };
    TEST_CASE(parse_opt_error_handling) {
        char* const argv[] = {(char*)"", (char*)"-p", nullptr};
        auto res = optdata::catter_proxy::parse_opt(argv);
        EXPECT_TRUE(!res.has_value());
        if(!res.has_value()) {
            catter::output::blueLn("Expected Error: {}", res.error());
        }
    };
    TEST_CASE(parse_opt_pass_an_err) {
        char* const argv[] = {(char*)"",
                              (char*)"-p",
                              (char*)"91011",
                              (char*)"--exec",
                              (char*)"/usr/bin/python3",
                              (char*)"report err!",
                              nullptr};
        auto res = optdata::catter_proxy::parse_opt(argv);
        EXPECT_TRUE(res.has_value());
        EXPECT_TRUE(!res->raw_argv_or_err.has_value());
        if(!res->raw_argv_or_err.has_value()) {
            catter::output::blueLn("Expected Error: {}", res->raw_argv_or_err.error());
        }
    };
};
