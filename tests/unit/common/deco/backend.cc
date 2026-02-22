#include "deco/backend.h"
#include "deco/macro.h"
#include <zest/zest.h>

#include <expected>
#include <string>
#include <vector>

namespace {
struct NestedOpt {
    DecoKV(help = "output"; required = false; category = 1;)<std::string> out_path;
    DecoComma(names = {"--T"}; required = false; category = 1;)<std::vector<std::string>> tags;
};

struct ParseAllOpt {
    // clang-format off
    DECO_CFG_START(required = false);
    DecoFlag(
        names = {"-V", "--version"};
        required = false;
        category = 2;
        exclusive = true;
    )
    verbose;
    DecoInput(
        help = "input";
        required = false;
    )<std::string>
    input;
    DecoKVStyled(
        deco::decl::KVStyle::Joined,
        required = false;
        category = 1;
    )<int> opt;
    DECO_CFG_END();

    DECO_CFG(category = 1; required = false;);
    NestedOpt nested;
    DecoMulti(2, {
        names = {"-P", "--pair"};
        required = false;
        category = 1;
        })<std::vector<std::string>>
    pair;

    // clang-format on
};

struct ParsePackOpt {
    DecoFlag() d;
    DecoPack(help = "pack"; required = false;)<std::vector<std::string>> pack = {};
};

struct RequiredOpt {
    DecoKV()<int> must;
};

struct DeepCfgInner {
    DECO_CFG_START(required = false; category = 22;);
    DecoKV()<int> a;
    DECO_CFG_END();
};

struct DeepCfgOpt {
    DECO_CFG_START(required = false; category = 11;);
    DecoKV()<int> top;
    DECO_CFG_START(required = false; category = 22;);
    DeepCfgInner inner;
    DecoKV()<int> mid;
    DECO_CFG_END();
    DecoKV()<int> tail;
    DECO_CFG_END();
};

using Parsed = catter::opt::ParsedArgument;
}  // namespace

TEST_SUITE(deco_backend){TEST_CASE(storage_keeps_dummy_alignment_for_id_map){
    const auto& built = deco::detail::build_storage<ParseAllOpt>();

EXPECT_TRUE(built.id_map().size() == built.option_infos().size() + 1);
EXPECT_TRUE(built.id_map()[0] == nullptr);
EXPECT_TRUE(built.option_infos().size() == built.opt_size());
for(size_t i = 0; i < built.option_infos().size(); ++i) {
    EXPECT_TRUE(built.option_infos()[i].id == i + 1);
    if(built.option_infos()[i].kind == catter::opt::Option::UnknownClass) {
        EXPECT_TRUE(built.id_map()[i + 1] == nullptr);
    } else {
        EXPECT_TRUE(built.id_map()[i + 1] != nullptr);
    }
}
}

TEST_CASE(parse_covers_flag_input_kv_comma_multi) {
    const auto& built = deco::detail::build_storage<ParseAllOpt>();
    auto table = built.make_opt_table();
    std::vector<std::string> argv = {"--version",
                                     "--opt42",
                                     "--out-path",
                                     "a.out",
                                     "--T,x,y",
                                     "-P",
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
    ParseAllOpt opt{};

    EXPECT_TRUE(args.size() == 8);

    EXPECT_TRUE(args[0].get_spelling_view() == "--version");
    EXPECT_TRUE(args[0].values.empty());
    EXPECT_TRUE(built.field_ptr_of(args[0].option_id, opt) == static_cast<void*>(&opt.verbose));

    EXPECT_TRUE(args[1].get_spelling_view() == "--opt");
    EXPECT_TRUE(args[1].values.size() == 1);
    EXPECT_TRUE(args[1].values[0] == "42");
    EXPECT_TRUE(built.field_ptr_of(args[1].option_id, opt) == static_cast<void*>(&opt.opt));

    EXPECT_TRUE(args[2].get_spelling_view() == "--out-path");
    EXPECT_TRUE(args[2].values.size() == 1);
    EXPECT_TRUE(args[2].values[0] == "a.out");
    EXPECT_TRUE(built.field_ptr_of(args[2].option_id, opt) ==
                static_cast<void*>(&opt.nested.out_path));

    EXPECT_TRUE(args[3].get_spelling_view() == "--T");
    EXPECT_TRUE(args[3].values.size() == 2);
    EXPECT_TRUE(args[3].values[0] == "x");
    EXPECT_TRUE(args[3].values[1] == "y");
    EXPECT_TRUE(built.field_ptr_of(args[3].option_id, opt) == static_cast<void*>(&opt.nested.tags));

    EXPECT_TRUE(args[4].get_spelling_view() == "-P");
    EXPECT_TRUE(args[4].values.size() == 2);
    EXPECT_TRUE(args[4].values[0] == "left");
    EXPECT_TRUE(args[4].values[1] == "right");
    EXPECT_TRUE(built.field_ptr_of(args[4].option_id, opt) == static_cast<void*>(&opt.pair));

    EXPECT_TRUE(args[5].get_spelling_view() == "main.cc");
    EXPECT_TRUE(args[5].values.empty());
    EXPECT_TRUE(built.field_ptr_of(args[5].option_id, opt) == static_cast<void*>(&opt.input));

    EXPECT_TRUE(args[6].get_spelling_view() == "tail1");
    EXPECT_TRUE(args[6].values.empty());
    EXPECT_TRUE(built.field_ptr_of(args[6].option_id, opt) == static_cast<void*>(&opt.input));

    EXPECT_TRUE(args[7].get_spelling_view() == "tail2");
    EXPECT_TRUE(args[7].values.empty());
    EXPECT_TRUE(built.field_ptr_of(args[7].option_id, opt) == static_cast<void*>(&opt.input));
}

TEST_CASE(parse_pack_covers_trailing_input_option) {
    const auto& built = deco::detail::build_storage<ParsePackOpt>();
    auto table = built.make_opt_table();
    std::vector<std::string> argv = {"-d", "--", "a", "b", "c"};

    std::vector<Parsed> args;
    table.parse_args(argv, [&](std::expected<Parsed, std::string> parsed) {
        EXPECT_TRUE(parsed.has_value());
        if(parsed.has_value()) {
            args.push_back(parsed.value());
        }
    });
    ParsePackOpt opt{};

    EXPECT_TRUE(args.size() == 2);

    EXPECT_TRUE(args[0].get_spelling_view() == "-d");
    EXPECT_TRUE(args[0].values.empty());
    EXPECT_TRUE(built.field_ptr_of(args[0].option_id, opt) == static_cast<void*>(&opt.d));

    EXPECT_TRUE(args[1].get_spelling_view() == "--");
    EXPECT_TRUE(args[1].values.size() == 3);
    EXPECT_TRUE(args[1].values[0] == "a");
    EXPECT_TRUE(args[1].values[1] == "b");
    EXPECT_TRUE(args[1].values[2] == "c");
    EXPECT_TRUE(built.field_ptr_of(args[1].option_id, opt) == static_cast<void*>(&opt.pack));
}

TEST_CASE(validate_checks_required_fields) {
    const auto& built = deco::detail::build_storage<RequiredOpt>();
    RequiredOpt opt{};

    auto err = built.validate(opt);
    EXPECT_TRUE(err.has_value());
    EXPECT_TRUE(err.value().find("must") != std::string::npos);
}

TEST_CASE(validate_checks_category_and_exclusive_rules) {
    const auto& built = deco::detail::build_storage<ParseAllOpt>();

    // cfg_start(required=false) makes verbose/input/opt category become 0.
    ParseAllOpt opt_only{};
    opt_only.opt.value = 1;
    EXPECT_TRUE(!built.validate(opt_only).has_value());

    ParseAllOpt nested_partial{};
    nested_partial.nested.out_path.value = "a.out";
    auto nested_partial_err = built.validate(nested_partial);
    EXPECT_TRUE(nested_partial_err.has_value());
    EXPECT_TRUE(nested_partial_err.value().find("category 1") != std::string::npos);

    ParseAllOpt with_all_categories{};
    with_all_categories.verbose.value = true;
    with_all_categories.opt.value = 1;
    with_all_categories.nested.out_path.value = "a.out";
    with_all_categories.nested.tags.value = std::vector<std::string>{"x", "y"};
    with_all_categories.pair.value = std::vector<std::string>{"left", "right"};
    EXPECT_TRUE(!built.validate(with_all_categories).has_value());

    ParseAllOpt valid_category{};
    valid_category.opt.value = 1;
    valid_category.nested.out_path.value = "a.out";
    valid_category.nested.tags.value = std::vector<std::string>{"x", "y"};
    valid_category.pair.value = std::vector<std::string>{"left", "right"};
    EXPECT_TRUE(!built.validate(valid_category).has_value());

    ParseAllOpt valid_exclusive{};
    valid_exclusive.verbose.value = true;
    EXPECT_TRUE(!built.validate(valid_exclusive).has_value());
}

TEST_CASE(validate_supports_deep_nested_cfg_areas) {
    const auto& built = deco::detail::build_storage<DeepCfgOpt>();

    DeepCfgOpt mid_only{};
    mid_only.mid.value = 1;
    auto mid_only_err = built.validate(mid_only);
    EXPECT_TRUE(mid_only_err.has_value());
    if(mid_only_err.has_value()) {
        EXPECT_TRUE(mid_only_err.value().find("category 22") != std::string::npos);
    }

    DeepCfgOpt inner_and_mid{};
    inner_and_mid.inner.a.value = 1;
    inner_and_mid.mid.value = 2;
    EXPECT_TRUE(!built.validate(inner_and_mid).has_value());

    DeepCfgOpt top_only{};
    top_only.top.value = 1;
    auto top_only_err = built.validate(top_only);
    EXPECT_TRUE(top_only_err.has_value());
    if(top_only_err.has_value()) {
        EXPECT_TRUE(top_only_err.value().find("category 11") != std::string::npos);
    }

    DeepCfgOpt all_valid{};
    all_valid.top.value = 1;
    all_valid.tail.value = 2;
    all_valid.inner.a.value = 3;
    all_valid.mid.value = 4;
    EXPECT_TRUE(!built.validate(all_valid).has_value());
}
}
;
