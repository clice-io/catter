#include "deco/backend.h"
#include "deco/macro.h"
#include <zest/zest.h>

#include <expected>
#include <string>
#include <vector>

namespace {
constexpr deco::decl::Category k_shared_category = {
    .exclusive = false,
    .name = "shared",
    .description = "all nested output options must be set together",
};

constexpr deco::decl::Category k_version_category = {
    .exclusive = true,
    .name = "version",
    .description = "version-only mode",
};

constexpr deco::decl::Category k_request_category = {
    .exclusive = true,
    .name = "request",
    .description = "request-only mode",
};

constexpr deco::decl::Category k_top_category = {
    .exclusive = false,
    .name = "top",
    .description = "top config group",
};

constexpr deco::decl::Category k_inner_category = {
    .exclusive = false,
    .name = "inner",
    .description = "nested config group",
};

constexpr deco::decl::Category k_input_category = {
    .exclusive = false,
    .name = "input",
    .description = "single positional input",
};

constexpr deco::decl::Category k_trailing_category = {
    .exclusive = false,
    .name = "trailing",
    .description = "arguments after --",
};

struct NestedOpt {
    DecoKV(help = "output"; required = false; category = k_shared_category;)<std::string> out_path;
    DecoComma(names = {"--T"}; required = false;
              category = k_shared_category;)<std::vector<std::string>> tags;
};

struct ParseAllOpt {
    // clang-format off
    DECO_CFG_START(required = false);
    DecoFlag(
        names = {"-V", "--version"};
        required = false;
        category = k_version_category;
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
        category = k_shared_category;
    )<int> opt;
    DECO_CFG_END();

    DECO_CFG(category = k_shared_category; required = false;);
    NestedOpt nested;
    DecoMulti(2, {
        names = {"-P", "--pair"};
        required = false;
        category = k_shared_category;
        })<std::vector<std::string>>
    pair;

    // clang-format on
};

struct ParsePackOpt {
    DecoFlag() d;
    DecoPack(help = "pack"; required = false;)<std::vector<std::string>> pack = {};
};

struct InputThenPackOpt {
    DecoInput(required = false; category = k_input_category;)<std::string> input;
    DecoPack(required = false; category = k_trailing_category;)<std::vector<std::string>> pack;
};

struct PackThenInputOpt {
    DecoPack(required = false; category = k_trailing_category;)<std::vector<std::string>> pack;
    DecoInput(required = false; category = k_input_category;)<std::string> input;
};

struct RequiredOpt {
    DecoKV(required = true; help = "required integer option";)<int> must;
};

struct DeepCfgInner {
    DECO_CFG_START(required = false; category = k_inner_category;);
    DecoKV()<int> a;
    DECO_CFG_END();
};

struct DeepCfgOpt {
    DECO_CFG_START(required = false; category = k_top_category;);
    DecoKV()<int> top;
    DECO_CFG_START(required = false; category = k_inner_category;);
    DeepCfgInner inner;
    DecoKV()<int> mid;
    DECO_CFG_END();
    DecoKV()<int> tail;
    DECO_CFG_END();
};

struct NextScopedNested {
    DecoKV()<int> left;
    DecoKV()<int> right;
};

struct NextOnNestedOpt {
    DECO_CFG(required = false; category = k_shared_category;);
    NextScopedNested nested;
    DecoKV(required = false;)<int> tail;
};

struct ExclusiveCategoryOpt {
    DecoFlag(required = false; category = k_version_category;) version;
    DecoFlag(required = false; category = k_shared_category;) shared;
};

struct MultiExclusiveCategoryOpt {
    DecoFlag(required = false; category = k_version_category;) version;
    DecoFlag(required = false; category = k_request_category;) request;
};

using Parsed = catter::opt::ParsedArgument;

template <typename BuiltTy>
std::expected<std::vector<Parsed>, std::string> parse_with(const BuiltTy& built,
                                                           std::vector<std::string> argv) {
    auto table = built.make_opt_table();
    std::vector<Parsed> args;
    std::string err;
    table.parse_args(argv, [&](std::expected<Parsed, std::string> parsed) {
        if(!parsed.has_value()) {
            if(err.empty()) {
                err = parsed.error();
            }
            return;
        }
        args.push_back(parsed.value());
    });
    if(!err.empty()) {
        return std::unexpected(err);
    }
    return args;
}
}  // namespace

TEST_SUITE(deco_backend){TEST_CASE(storage_keeps_dummy_alignment_for_id_map){
    const auto& built = deco::detail::build_storage<ParseAllOpt>();

EXPECT_TRUE(built.id_map().size() == built.option_infos().size() + 1);
EXPECT_TRUE(built.category_map().size() == built.id_map().size());
EXPECT_TRUE(built.id_map()[0] == nullptr);
EXPECT_TRUE(built.category_map()[0] == nullptr);
EXPECT_TRUE(built.option_infos().size() == built.opt_size());
for(size_t i = 0; i < built.option_infos().size(); ++i) {
    EXPECT_TRUE(built.option_infos()[i].id == i + 1);
    if(built.option_infos()[i].kind == catter::opt::Option::UnknownClass) {
        EXPECT_TRUE(built.id_map()[i + 1] == nullptr);
        EXPECT_TRUE(built.category_map()[i + 1] == nullptr);
    } else {
        EXPECT_TRUE(built.id_map()[i + 1] != nullptr);
        EXPECT_TRUE(built.category_map()[i + 1] != nullptr);
    }
}
}

TEST_CASE(parse_covers_flag_input_kv_comma_multi) {
    const auto& built = deco::detail::build_storage<ParseAllOpt>();
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

    auto parsed_args = parse_with(built, argv);
    EXPECT_TRUE(parsed_args.has_value());
    if(!parsed_args.has_value()) {
        return;
    }
    auto args = std::move(parsed_args.value());
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
    EXPECT_TRUE(built.category_of(args[2].option_id) == &k_shared_category);

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
    std::vector<std::string> argv = {"-d", "--", "a", "b", "c"};

    auto parsed_args = parse_with(built, argv);
    EXPECT_TRUE(parsed_args.has_value());
    if(!parsed_args.has_value()) {
        return;
    }
    auto args = std::move(parsed_args.value());
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

TEST_CASE(parse_input_and_pack_can_coexist) {
    const auto& built = deco::detail::build_storage<InputThenPackOpt>();
    auto parsed_args = parse_with(built, {"front", "--", "a", "b"});
    EXPECT_TRUE(parsed_args.has_value());
    if(!parsed_args.has_value()) {
        return;
    }
    auto args = std::move(parsed_args.value());
    InputThenPackOpt opt{};

    EXPECT_TRUE(args.size() == 2);
    EXPECT_TRUE(!built.is_trailing_argument(args[0]));
    EXPECT_TRUE(built.field_ptr_of(args[0].option_id, opt) == static_cast<void*>(&opt.input));
    EXPECT_TRUE(built.category_of(args[0].option_id) == &k_input_category);

    EXPECT_TRUE(built.is_trailing_argument(args[1]));
    EXPECT_TRUE(built.trailing_ptr_of(opt) == static_cast<void*>(&opt.pack));
    EXPECT_TRUE(built.trailing_category() == &k_trailing_category);
}

TEST_CASE(parse_pack_then_input_rebinds_input_id_map) {
    const auto& built = deco::detail::build_storage<PackThenInputOpt>();
    auto parsed_args = parse_with(built, {"front", "--", "a", "b"});
    EXPECT_TRUE(parsed_args.has_value());
    if(!parsed_args.has_value()) {
        return;
    }
    auto args = std::move(parsed_args.value());
    PackThenInputOpt opt{};

    EXPECT_TRUE(args.size() == 2);
    EXPECT_TRUE(!built.is_trailing_argument(args[0]));
    EXPECT_TRUE(built.field_ptr_of(args[0].option_id, opt) == static_cast<void*>(&opt.input));
    EXPECT_TRUE(built.category_of(args[0].option_id) == &k_input_category);

    EXPECT_TRUE(built.is_trailing_argument(args[1]));
    EXPECT_TRUE(built.field_ptr_of(args[1].option_id, opt) == static_cast<void*>(&opt.input));
    EXPECT_TRUE(built.trailing_ptr_of(opt) == static_cast<void*>(&opt.pack));
    EXPECT_TRUE(built.trailing_category() == &k_trailing_category);
}

TEST_CASE(category_map_assigns_expected_categories_for_parsed_args) {
    const auto& built = deco::detail::build_storage<ParseAllOpt>();
    auto parsed_args = parse_with(built,
                                  {"--version",
                                   "--opt1",
                                   "main.cc",
                                   "--out-path",
                                   "a.out",
                                   "--T,x,y",
                                   "-P",
                                   "left",
                                   "right"});
    EXPECT_TRUE(parsed_args.has_value());
    if(!parsed_args.has_value()) {
        return;
    }
    const auto& args = parsed_args.value();
    EXPECT_TRUE(args.size() == 6);

    std::size_t default_count = 0;
    std::size_t shared_count = 0;
    std::size_t version_count = 0;
    for(const auto& arg: args) {
        const auto* category = built.category_of(arg.option_id);
        EXPECT_TRUE(category != nullptr);
        const auto spelling = arg.get_spelling_view();
        if(spelling == "--version") {
            EXPECT_TRUE(category == &k_version_category);
            version_count += 1;
        } else if(spelling == "main.cc") {
            EXPECT_TRUE(category == &deco::decl::default_category);
            default_count += 1;
        } else {
            EXPECT_TRUE(category == &k_shared_category);
            shared_count += 1;
        }
    }
    EXPECT_TRUE(default_count == 1);
    EXPECT_TRUE(shared_count == 4);
    EXPECT_TRUE(version_count == 1);
}

TEST_CASE(category_map_keeps_alias_category_consistent) {
    const auto& built = deco::detail::build_storage<ParseAllOpt>();
    auto short_args = parse_with(built, {"-V"});
    auto long_args = parse_with(built, {"--version"});
    EXPECT_TRUE(short_args.has_value());
    EXPECT_TRUE(long_args.has_value());
    if(!short_args.has_value() || !long_args.has_value()) {
        return;
    }
    EXPECT_TRUE(short_args->size() == 1);
    EXPECT_TRUE(long_args->size() == 1);
    if(short_args->size() != 1 || long_args->size() != 1) {
        return;
    }
    EXPECT_TRUE(built.category_of((*short_args)[0].option_id) == &k_version_category);
    EXPECT_TRUE(built.category_of((*long_args)[0].option_id) == &k_version_category);
}

TEST_CASE(category_map_supports_deep_nested_cfg_areas) {
    const auto& built = deco::detail::build_storage<DeepCfgOpt>();
    auto parsed_args = parse_with(built, {"--top", "1", "--tail", "2", "-a", "3", "--mid", "4"});
    EXPECT_TRUE(parsed_args.has_value());
    if(!parsed_args.has_value()) {
        return;
    }
    const auto& args = parsed_args.value();
    EXPECT_TRUE(args.size() == 4);
    std::size_t top_count = 0;
    std::size_t inner_count = 0;
    for(const auto& arg: args) {
        const auto* category = built.category_of(arg.option_id);
        EXPECT_TRUE(category != nullptr);
        const auto spelling = arg.get_spelling_view();
        if(spelling == "--top" || spelling == "--tail") {
            EXPECT_TRUE(category == &k_top_category);
            top_count += 1;
        } else if(spelling == "-a" || spelling == "--mid") {
            EXPECT_TRUE(category == &k_inner_category);
            inner_count += 1;
        }
    }
    EXPECT_TRUE(top_count == 2);
    EXPECT_TRUE(inner_count == 2);
}

TEST_CASE(category_map_supports_multiple_exclusive_category_definitions) {
    const auto& built = deco::detail::build_storage<MultiExclusiveCategoryOpt>();
    auto parsed_args = parse_with(built, {"--version", "--request"});
    EXPECT_TRUE(parsed_args.has_value());
    if(!parsed_args.has_value()) {
        return;
    }
    EXPECT_TRUE(parsed_args->size() == 2);
    if(parsed_args->size() != 2) {
        return;
    }
    EXPECT_TRUE(built.category_of((*parsed_args)[0].option_id) == &k_version_category);
    EXPECT_TRUE(built.category_of((*parsed_args)[1].option_id) == &k_request_category);
}

TEST_CASE(visit_fields_applies_next_cfg_to_nested_struct_fields) {
    const auto& built = deco::detail::build_storage<NextOnNestedOpt>();

    auto partial_nested_args = parse_with(built, {"--left", "1"});
    EXPECT_TRUE(partial_nested_args.has_value());
    if(!partial_nested_args.has_value()) {
        return;
    }

    EXPECT_TRUE(!partial_nested_args.value().empty());
    EXPECT_TRUE(built.category_of(partial_nested_args.value()[0].option_id) == &k_shared_category);

    NextOnNestedOpt default_opt{};
    std::size_t nested_cfg_count = 0;
    std::size_t tail_cfg_count = 0;
    const bool visited =
        built.visit_fields(default_opt,
                           [&](const auto&, const auto& cfg, std::string_view field_name, auto) {
                               if(field_name == "left" || field_name == "right") {
                                   EXPECT_TRUE(cfg.required == false);
                                   EXPECT_TRUE(cfg.category.ptr() == &k_shared_category);
                                   nested_cfg_count += 1;
                               }
                               if(field_name == "tail") {
                                   EXPECT_TRUE(cfg.required == false);
                                   EXPECT_TRUE(cfg.category.ptr() == &deco::decl::default_category);
                                   tail_cfg_count += 1;
                               }
                               return true;
                           });
    EXPECT_TRUE(visited);
    EXPECT_TRUE(nested_cfg_count == 2);
    EXPECT_TRUE(tail_cfg_count == 1);
}
}
;
