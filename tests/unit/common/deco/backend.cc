#include "deco/backend.h"
#include "deco/macro.h"
#include <zest/zest.h>

#include <expected>
#include <string>
#include <vector>

namespace {
struct ParseAllOpt {
    DecoFlag(verbose, prefix = deco::decl::Prefix::Dash; alias = {"V"};) verbose;
    DecoInput(help = "input")<std::string> input;
    DecoKVStyled(opt, deco::decl::KVStyle::Joined, prefix = deco::decl::Prefix::Dash;)<int> opt;
    DecoKV(out, help = "output";)<std::string> out;
    DecoComma(tags, alias = {"T"};)<std::vector<std::string>> tags;
    DecoMulti(pair, 2, alias = {"P"};)<std::vector<std::string>> pair;
};

struct ParsePackOpt {
    DecoFlag(debug, prefix = deco::decl::Prefix::Dash;) debug;
    DecoPack(help = "pack")<std::vector<std::string>> pack;
};

using Parsed = catter::opt::ParsedArgument;
}  // namespace

TEST_SUITE(deco_backend){TEST_CASE(storage_keeps_dummy_alignment_for_id_map){
    const auto& built = deco::detail::build_storage<ParseAllOpt>();

EXPECT_TRUE(built.id_map().size() == built.option_infos().size() + 1);
EXPECT_TRUE(built.id_map()[0] == deco::detail::no_struct_index);
EXPECT_TRUE(built.option_infos().size() == built.opt_size());
for(size_t i = 0; i < built.option_infos().size(); ++i) {
    EXPECT_TRUE(built.option_infos()[i].id == i + 1);
}
}

TEST_CASE(parse_covers_flag_input_kv_comma_multi) {
    const auto& built = deco::detail::build_storage<ParseAllOpt>();
    auto table = built.make_opt_table();
    std::vector<std::string> argv = {"-V",
                                     "-opt42",
                                     "--out",
                                     "a.out",
                                     "--T,x,y",
                                     "--pair",
                                     "left",
                                     "right",
                                     "main.cc",
                                     "--",
                                     "tail1",
                                     "tail2"};

    std::vector<Parsed> args;
    table.parse_args(argv, [&](std::expected<Parsed, std::string> parsed) {
        EXPECT_TRUE(parsed.has_value());
        if(parsed.has_value()) {
            args.push_back(parsed.value());
        }
    });

    EXPECT_TRUE(args.size() == 8);

    EXPECT_TRUE(args[0].get_spelling_view() == "-V");
    EXPECT_TRUE(args[0].values.empty());
    EXPECT_TRUE(built.struct_index_of(args[0].option_id) == 0);

    EXPECT_TRUE(args[1].get_spelling_view() == "-opt");
    EXPECT_TRUE(args[1].values.size() == 1);
    EXPECT_TRUE(args[1].values[0] == "42");
    EXPECT_TRUE(built.struct_index_of(args[1].option_id) == 2);

    EXPECT_TRUE(args[2].get_spelling_view() == "--out");
    EXPECT_TRUE(args[2].values.size() == 1);
    EXPECT_TRUE(args[2].values[0] == "a.out");
    EXPECT_TRUE(built.struct_index_of(args[2].option_id) == 3);

    EXPECT_TRUE(args[3].get_spelling_view() == "--T");
    EXPECT_TRUE(args[3].values.size() == 2);
    EXPECT_TRUE(args[3].values[0] == "x");
    EXPECT_TRUE(args[3].values[1] == "y");
    EXPECT_TRUE(built.struct_index_of(args[3].option_id) == 4);

    EXPECT_TRUE(args[4].get_spelling_view() == "--pair");
    EXPECT_TRUE(args[4].values.size() == 2);
    EXPECT_TRUE(args[4].values[0] == "left");
    EXPECT_TRUE(args[4].values[1] == "right");
    EXPECT_TRUE(built.struct_index_of(args[4].option_id) == 5);

    EXPECT_TRUE(args[5].get_spelling_view() == "main.cc");
    EXPECT_TRUE(args[5].values.empty());
    EXPECT_TRUE(built.struct_index_of(args[5].option_id) == 1);

    EXPECT_TRUE(args[6].get_spelling_view() == "tail1");
    EXPECT_TRUE(args[6].values.empty());
    EXPECT_TRUE(built.struct_index_of(args[6].option_id) == 1);

    EXPECT_TRUE(args[7].get_spelling_view() == "tail2");
    EXPECT_TRUE(args[7].values.empty());
    EXPECT_TRUE(built.struct_index_of(args[7].option_id) == 1);
}

TEST_CASE(parse_pack_covers_trailing_input_option) {
    const auto& built = deco::detail::build_storage<ParsePackOpt>();
    auto table = built.make_opt_table();
    std::vector<std::string> argv = {"-debug", "--", "a", "b", "c"};

    std::vector<Parsed> args;
    table.parse_args(argv, [&](std::expected<Parsed, std::string> parsed) {
        EXPECT_TRUE(parsed.has_value());
        if(parsed.has_value()) {
            args.push_back(parsed.value());
        }
    });

    EXPECT_TRUE(args.size() == 2);

    EXPECT_TRUE(args[0].get_spelling_view() == "-debug");
    EXPECT_TRUE(args[0].values.empty());
    EXPECT_TRUE(built.struct_index_of(args[0].option_id) == 0);

    EXPECT_TRUE(args[1].get_spelling_view() == "--");
    EXPECT_TRUE(args[1].values.size() == 3);
    EXPECT_TRUE(args[1].values[0] == "a");
    EXPECT_TRUE(args[1].values[1] == "b");
    EXPECT_TRUE(args[1].values[2] == "c");
    EXPECT_TRUE(built.struct_index_of(args[1].option_id) == 1);
}
}
;
