module;

#include "modules/prelude.h"

module catter;

import catter.command;

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
    for(auto parsed: option::clang::table().parse(args, options)) {
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
    auto option = option::clang::table().option(arg.id);
    if(!option.has_value()) {
        return arg.spelling;
    }
    return option->unaliased_option().prefixed_name();
}

}  // namespace

ZEST_SUITE(clang_option_table_tests) {
ZEST_CASE(parse_clang_compile_command) {
    const auto argv = std::to_array<std::string>(
        {"clang++", "-c", "main.cc", "-Iinclude", "-isystem", "/usr/include", "-o", "main.o"});

    auto parsed = parse_command(argv);

    ZEXPECT(parsed.errors.empty());
    ZASSERT(parsed.args.size() == 5U);

    ZEXPECT(parsed.args[0].id == option::clang::ID_c);

    ZEXPECT(parsed.args[1].id == option::clang::ID_INPUT);
    ZEXPECT(parsed.args[1].spelling == "main.cc");

    ZEXPECT(parsed.args[2].id == option::clang::ID_I);
    ZASSERT(parsed.args[2].values.size() == 1U);
    ZEXPECT(parsed.args[2].values[0] == "include");

    ZEXPECT(parsed.args[3].id == option::clang::ID_isystem);
    ZASSERT(parsed.args[3].values.size() == 1U);
    ZEXPECT(parsed.args[3].values[0] == "/usr/include");

    ZEXPECT(parsed.args[4].id == option::clang::ID_o);
    ZASSERT(parsed.args[4].values.size() == 1U);
    ZEXPECT(parsed.args[4].values[0] == "main.o");
};

ZEST_CASE(parse_alias_and_dash_dash_inputs) {
    const auto argv = std::to_array<std::string>(
        {"clang++", "--all-warnings", "-fsyntax-only", "--", "-dash.cc"});

    auto parsed = parse_command(argv);

    ZEXPECT(parsed.errors.empty());
    ZASSERT(parsed.args.size() == 3U);

    ZEXPECT(canonical_spelling(parsed.args[0]) == "-Wall");
    ZEXPECT(parsed.args[0].id == option::clang::ID_Wall);

    ZEXPECT(parsed.args[1].id == option::clang::ID_fsyntax_only);

    ZEXPECT(parsed.args[2].id == option::clang::ID_INPUT);
    ZEXPECT(parsed.args[2].spelling == "-dash.cc");
};

ZEST_CASE(parse_unknown_and_missing_value) {

    {
        const auto argv =
            std::to_array<std::string>({"clang++", "--definitely-not-a-real-clang-flag"});
        auto parsed = parse_command(argv);
        ZEXPECT(parsed.errors.empty());
        ZASSERT(parsed.args.size() == 1U);
        ZEXPECT(parsed.args[0].id == option::clang::ID_UNKNOWN);
        ZEXPECT(parsed.args[0].spelling == "--definitely-not-a-real-clang-flag");
    };

    {
        const auto argv = std::to_array<std::string>({"clang++", "-o"});
        auto parsed = parse_command(argv);
        ZEXPECT(parsed.args.empty());
        ZASSERT(parsed.errors.size() == 1U);
        ZEXPECT(parsed.errors[0].contains("missing argument value"));
    };
}

ZEST_CASE(parse_clang_cl_output_options_with_cl_visibility) {
    const auto argv = std::to_array<std::string>(
        {"clang-cl", "/c", "main.cc", "/Foobj/main.obj", "/Fe:bin/tool.exe"});

    auto parsed = parse_command(argv, option::clang::DefaultVis | option::clang::CLOption);

    ZEXPECT(parsed.errors.empty());
    ZASSERT(parsed.args.size() == 4U);

    ZEXPECT(parsed.args[0].id == option::clang::ID_c);
    ZEXPECT(parsed.args[0].spelling == "/c");

    ZEXPECT(parsed.args[1].id == option::clang::ID_INPUT);
    ZEXPECT(parsed.args[1].spelling == "main.cc");

    ZEXPECT(parsed.args[2].id == option::clang::ID__SLASH_Fo);
    ZASSERT(parsed.args[2].values.size() == 1U);
    ZEXPECT(parsed.args[2].values[0] == "obj/main.obj");

    ZEXPECT(parsed.args[3].id == option::clang::ID__SLASH_Fe);
    ZASSERT(parsed.args[3].values.size() == 1U);
    ZEXPECT(parsed.args[3].values[0] == "bin/tool.exe");
};
};  // ZEST_SUITE(clang_option_table_tests)
