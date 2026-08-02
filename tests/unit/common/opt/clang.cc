#include "opt/external/clang.h"

#include <array>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <kota/zest/macro.h>
#include <kota/zest/zest.h>
#include <kota/deco/option.h>

using namespace catter;

namespace {

std::vector<std::string> copy_values(std::span<const std::string_view> values) {
    std::vector<std::string> copied;
    copied.reserve(values.size());
    for(auto value: values) {
        copied.emplace_back(value);
    }
    return copied;
}

struct ParseResult {
    struct OwnedArg {
        std::uint32_t id;
        std::string spelling;
        std::vector<std::string> values;
        std::uint32_t index;
    };

    std::vector<OwnedArg> args;
    std::vector<std::string> errors;
};

ParseResult parse_command(std::span<const std::string> argv, uint32_t visibility = 0xffffffffU) {
    std::vector<std::string> args(argv.begin() + 1, argv.end());

    ParseResult result;
    kota::option::ParseOptions options;
    options.dash_dash_parsing = true;
    options.visibility = visibility;
    for(auto parsed: opt::clang::table().parse(args, options)) {
        if(parsed.has_value()) {
            result.args.emplace_back(ParseResult::OwnedArg{
                .id = parsed->id,
                .spelling = parsed->spelling,
                .values = copy_values(parsed->values),
                .index = parsed->index,
            });
        } else {
            result.errors.emplace_back(parsed.error().message);
        }
    }
    return result;
}

std::string_view canonical_spelling(const ParseResult::OwnedArg& arg) {
    auto option = opt::clang::table().option(arg.id);
    if(!option.has_value()) {
        return arg.spelling;
    }
    return option->unaliased_option().prefixed_name();
}

}  // namespace

TEST_SUITE(clang_option_table_tests) {
TEST_CASE(parse_clang_compile_command) {
    const auto argv = std::to_array<std::string>(
        {"clang++", "-c", "main.cc", "-Iinclude", "-isystem", "/usr/include", "-o", "main.o"});

    auto parsed = parse_command(argv);

    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 5U);

    EXPECT_EQ(parsed.args[0].id, opt::clang::ID_c);

    EXPECT_EQ(parsed.args[1].id, opt::clang::ID_INPUT);
    EXPECT_EQ(parsed.args[1].spelling, "main.cc");

    EXPECT_EQ(parsed.args[2].id, opt::clang::ID_I);
    ASSERT_EQ(parsed.args[2].values.size(), 1U);
    EXPECT_EQ(parsed.args[2].values[0], "include");

    EXPECT_EQ(parsed.args[3].id, opt::clang::ID_isystem);
    ASSERT_EQ(parsed.args[3].values.size(), 1U);
    EXPECT_EQ(parsed.args[3].values[0], "/usr/include");

    EXPECT_EQ(parsed.args[4].id, opt::clang::ID_o);
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
    EXPECT_EQ(parsed.args[0].id, opt::clang::ID_Wall);

    EXPECT_EQ(parsed.args[1].id, opt::clang::ID_fsyntax_only);

    EXPECT_EQ(parsed.args[2].id, opt::clang::ID_INPUT);
    EXPECT_EQ(parsed.args[2].spelling, "-dash.cc");
};

TEST_CASE(parse_unknown_and_missing_value) {

    {
        const auto argv =
            std::to_array<std::string>({"clang++", "--definitely-not-a-real-clang-flag"});
        auto parsed = parse_command(argv);
        EXPECT_TRUE(parsed.errors.empty());
        ASSERT_EQ(parsed.args.size(), 1U);
        EXPECT_EQ(parsed.args[0].id, opt::clang::ID_UNKNOWN);
        EXPECT_EQ(parsed.args[0].spelling, "--definitely-not-a-real-clang-flag");
    };

    {
        const auto argv = std::to_array<std::string>({"clang++", "-o"});
        auto parsed = parse_command(argv);
        EXPECT_TRUE(parsed.args.empty());
        ASSERT_EQ(parsed.errors.size(), 1U);
        EXPECT_TRUE(parsed.errors[0].contains("missing argument value"));
    };
}

TEST_CASE(parse_clang_cl_output_options_with_cl_visibility) {
    const auto argv = std::to_array<std::string>(
        {"clang-cl", "/c", "main.cc", "/Foobj/main.obj", "/Fe:bin/tool.exe"});

    auto parsed = parse_command(argv, opt::clang::DefaultVis | opt::clang::CLOption);

    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 4U);

    EXPECT_EQ(parsed.args[0].id, opt::clang::ID_c);
    EXPECT_EQ(parsed.args[0].spelling, "/c");

    EXPECT_EQ(parsed.args[1].id, opt::clang::ID_INPUT);
    EXPECT_EQ(parsed.args[1].spelling, "main.cc");

    EXPECT_EQ(parsed.args[2].id, opt::clang::ID__SLASH_Fo);
    ASSERT_EQ(parsed.args[2].values.size(), 1U);
    EXPECT_EQ(parsed.args[2].values[0], "obj/main.obj");

    EXPECT_EQ(parsed.args[3].id, opt::clang::ID__SLASH_Fe);
    ASSERT_EQ(parsed.args[3].values.size(), 1U);
    EXPECT_EQ(parsed.args[3].values[0], "bin/tool.exe");
};
};  // TEST_SUITE(clang_option_table_tests)
