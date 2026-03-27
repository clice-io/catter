#include <array>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <eventide/option/option.h>
#include <eventide/zest/macro.h>
#include <eventide/zest/zest.h>

#include "opt/external/clang.h"

using namespace catter;

namespace {

struct ParseResult {
    std::vector<eventide::option::ParsedArgumentOwning> args;
    std::vector<std::string> errors;
};

ParseResult parse_command(std::span<const std::string> argv) {
    std::vector<std::string> args(argv.begin() + 1, argv.end());

    ParseResult result;
    opt::clang::table().parse_args(
        args,
        [&](std::expected<eventide::option::ParsedArgument, std::string> parsed) {
            if(parsed.has_value()) {
                result.args.emplace_back(
                    eventide::option::ParsedArgumentOwning::from_parsed_argument(*parsed));
            } else {
                result.errors.emplace_back(parsed.error());
            }
        });
    return result;
}

std::string_view canonical_spelling(const eventide::option::ParsedArgumentOwning& arg) {
    auto option = opt::clang::table().option(arg.unaliased_opt());
    if(!option.valid()) {
        return arg.get_spelling_view();
    }
    return option.prefixed_name();
}

}  // namespace

TEST_SUITE(clang_option_table_tests) {
TEST_CASE(parse_clang_compile_command) {
    const auto argv = std::to_array<std::string>(
        {"clang++", "-c", "main.cc", "-Iinclude", "-isystem", "/usr/include", "-o", "main.o"});

    auto parsed = parse_command(argv);

    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 5U);

    EXPECT_EQ(parsed.args[0].option_id.id(), opt::clang::ID_c);

    EXPECT_EQ(parsed.args[1].option_id.id(), opt::clang::ID_INPUT);
    EXPECT_EQ(parsed.args[1].get_spelling_view(), "main.cc");

    EXPECT_EQ(parsed.args[2].option_id.id(), opt::clang::ID_I);
    ASSERT_EQ(parsed.args[2].values.size(), 1U);
    EXPECT_EQ(parsed.args[2].values[0], "include");

    EXPECT_EQ(parsed.args[3].option_id.id(), opt::clang::ID_isystem);
    ASSERT_EQ(parsed.args[3].values.size(), 1U);
    EXPECT_EQ(parsed.args[3].values[0], "/usr/include");

    EXPECT_EQ(parsed.args[4].option_id.id(), opt::clang::ID_o);
    ASSERT_EQ(parsed.args[4].values.size(), 1U);
    EXPECT_EQ(parsed.args[4].values[0], "main.o");
};

TEST_CASE(parse_alias_and_dash_dash_inputs) {
    const auto argv = std::to_array<std::string>(
        {"clang++", "--all-warnings", "-fsyntax-only", "--", "-dash.cc"});

    auto parsed = parse_command(argv);

    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 3U);

    EXPECT_EQ(canonical_spelling(parsed.args[0]), "-Wall");
    EXPECT_EQ(parsed.args[0].unaliased_opt().id(), opt::clang::ID_Wall);

    EXPECT_EQ(parsed.args[1].option_id.id(), opt::clang::ID_fsyntax_only);

    EXPECT_EQ(parsed.args[2].option_id.id(), opt::clang::ID_INPUT);
    EXPECT_EQ(parsed.args[2].get_spelling_view(), "-dash.cc");
};

TEST_CASE(parse_unknown_and_missing_value) {

    {
        const auto argv =
            std::to_array<std::string>({"clang++", "--definitely-not-a-real-clang-flag"});
        auto parsed = parse_command(argv);
        EXPECT_TRUE(parsed.errors.empty());
        ASSERT_EQ(parsed.args.size(), 1U);
        EXPECT_EQ(parsed.args[0].option_id.id(), opt::clang::ID_UNKNOWN);
        EXPECT_EQ(parsed.args[0].get_spelling_view(), "--definitely-not-a-real-clang-flag");
    };

    {
        const auto argv = std::to_array<std::string>({"clang++", "-o"});
        auto parsed = parse_command(argv);
        EXPECT_TRUE(parsed.args.empty());
        ASSERT_EQ(parsed.errors.size(), 1U);
        EXPECT_TRUE(parsed.errors[0].contains("missing argument value"));
    };
}
};  // TEST_SUITE(clang_option_table_tests)
