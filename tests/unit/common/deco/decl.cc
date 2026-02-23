#include "deco/decl.h"
#include "deco/macro.h"
#include "deco/trait.h"
#include <string>
#include <vector>
#include <zest/zest.h>

static_assert(deco::trait::ScalarResultType<bool>);
static_assert(deco::trait::ScalarResultType<int>);
static_assert(deco::trait::ScalarResultType<std::string>);
static_assert(!deco::trait::ScalarResultType<std::vector<int>>);

static_assert(deco::trait::VectorResultType<std::vector<int>>);
static_assert(deco::trait::VectorResultType<std::vector<std::string>>);
static_assert(!deco::trait::VectorResultType<std::span<const std::string>>);

constexpr deco::decl::Category k_verbose_category = {
    .exclusive = true,
    .name = "verbose",
    .description = "verbose-only mode",
};

constexpr deco::decl::Category k_pack_category = {
    .exclusive = false,
    .name = "pack",
    .description = "pack option group",
};

struct CustomScalarResult {
    std::string value;

    std::optional<std::string_view> into(std::string_view input) {
        value = std::string(input);
        return std::nullopt;
    }
};

struct CustomVectorResult {
    std::vector<std::string> values;

    std::optional<std::string_view> into(const std::vector<std::string_view>& input) {
        values.assign(input.begin(), input.end());
        return std::nullopt;
    }
};

auto make_parsed_arg(std::string_view spelling, std::vector<std::string_view> values = {}) {
    return catter::opt::ParsedArgument{
        .option_id = catter::opt::OptSpecifier(1u),
        .spelling = spelling,
        .values = std::move(values),
        .index = 0,
    };
}

struct DeclOpt {
    DecoFlag({
        help = "flag";
        required = false;
        category = k_verbose_category;
    }) verbose = true;

    DECO_CFG(required = true);
    DecoInput(help = "input")<int> input = 42;

    DecoPack(help = "pack"; category = k_pack_category;)<std::vector<std::string>> pack =
        std::vector<std::string> {
            "a",
            "b"
        };

    DecoKVStyled(deco::decl::KVStyle::Joined, help = "joined-kv";)<int> joined = 7;
    DecoKV(help = "separate-kv";)<std::string> path = "entry.js";
    DecoComma(help = "comma";
              names = {"-T"};)<std::vector<std::string>> tags = std::vector<std::string> {
        "x",
        "y"
    };
    DecoMulti(2, help = "multi"; names = {"-P"};)<std::vector<int>> pair = std::vector<int> {
        1,
        2
    };
};

static_assert(std::is_same_v<decltype(DeclOpt{}.pack)::result_type, std::vector<std::string>>);
static_assert(std::is_base_of_v<deco::decl::DecoOptionBase, decltype(DeclOpt{}.input)>);

TEST_SUITE(deco_decl){
    TEST_CASE(option_declaration_has_expected_shape_and_default_assignment){DeclOpt opt{};

using VerboseCfg = typename decltype(opt.verbose)::__cfg_ty;
VerboseCfg verbose_cfg{};
EXPECT_TRUE(verbose_cfg.names.empty());
EXPECT_TRUE(verbose_cfg.required == false);
EXPECT_TRUE(verbose_cfg.category->exclusive == true);
EXPECT_TRUE(verbose_cfg.category.ptr() == &k_verbose_category);
EXPECT_TRUE(opt.verbose.value.has_value());
EXPECT_TRUE(opt.verbose.value.value() == true);
opt.verbose = false;
EXPECT_TRUE(opt.verbose.value.has_value());
EXPECT_TRUE(opt.verbose.value.value() == false);

EXPECT_TRUE(opt.input.value.has_value());
EXPECT_TRUE(opt.input.value.value() == 42);
opt.input = 64;
EXPECT_TRUE(opt.input.value.value() == 64);

using PackCfg = typename decltype(opt.pack)::__cfg_ty;
PackCfg pack_cfg{};
EXPECT_TRUE(opt.pack.value.has_value());
EXPECT_TRUE(opt.pack.value.value().size() == 2);
EXPECT_TRUE(opt.pack.value.value()[0] == "a");
EXPECT_TRUE(opt.pack.value.value()[1] == "b");
EXPECT_TRUE(pack_cfg.category.ptr() == &k_pack_category);
opt.pack = std::vector<std::string>{"tail"};
EXPECT_TRUE(opt.pack.value.value().size() == 1);
EXPECT_TRUE(opt.pack.value.value()[0] == "tail");

using JoinedCfg = typename decltype(opt.joined)::__cfg_ty;
JoinedCfg joined_cfg{};
EXPECT_TRUE(joined_cfg.style == deco::decl::KVStyle::Joined);
EXPECT_TRUE(opt.joined.value.has_value());
EXPECT_TRUE(opt.joined.value.value() == 7);
opt.joined = 11;
EXPECT_TRUE(opt.joined.value.value() == 11);

using PathCfg = typename decltype(opt.path)::__cfg_ty;
PathCfg path_cfg{};
EXPECT_TRUE(path_cfg.style == deco::decl::KVStyle::Separate);
EXPECT_TRUE(opt.path.value.has_value());
EXPECT_TRUE(opt.path.value.value() == "entry.js");
opt.path = std::string("run.js");
EXPECT_TRUE(opt.path.value.value() == "run.js");

using TagsCfg = typename decltype(opt.tags)::__cfg_ty;
TagsCfg tags_cfg{};
EXPECT_TRUE(tags_cfg.names.size() == 1);
EXPECT_TRUE(tags_cfg.names[0] == "-T");
EXPECT_TRUE(opt.tags.value.has_value());
EXPECT_TRUE(opt.tags.value.value().size() == 2);
EXPECT_TRUE(opt.tags.value.value()[0] == "x");
EXPECT_TRUE(opt.tags.value.value()[1] == "y");
opt.tags = std::vector<std::string>{"only"};
EXPECT_TRUE(opt.tags.value.value().size() == 1);
EXPECT_TRUE(opt.tags.value.value()[0] == "only");

using PairCfg = typename decltype(opt.pair)::__cfg_ty;
PairCfg pair_cfg{};
EXPECT_TRUE(pair_cfg.arg_num == 2);
EXPECT_TRUE(pair_cfg.names.size() == 1);
EXPECT_TRUE(pair_cfg.names[0] == "-P");
EXPECT_TRUE(opt.pair.value.has_value());
EXPECT_TRUE(opt.pair.value.value().size() == 2);
EXPECT_TRUE(opt.pair.value.value()[0] == 1);
EXPECT_TRUE(opt.pair.value.value()[1] == 2);
opt.pair = std::vector<int>{9, 8};
EXPECT_TRUE(opt.pair.value.value().size() == 2);
EXPECT_TRUE(opt.pair.value.value()[0] == 9);
EXPECT_TRUE(opt.pair.value.value()[1] == 8);
}

TEST_CASE(option_into_assigns_values_by_option_kind) {
    deco::decl::FlagOption<bool> flag{};
    auto flag_ok = flag.into(make_parsed_arg("--verbose"));
    EXPECT_TRUE(!flag_ok.has_value());
    EXPECT_TRUE(flag.value.has_value());
    EXPECT_TRUE(flag.value.value() == true);
    auto flag_err = flag.into(make_parsed_arg("--verbose", {"1"}));
    EXPECT_TRUE(flag_err.has_value());

    deco::decl::ScalarOption<int> scalar{};
    auto scalar_ok = scalar.into(make_parsed_arg("--count", {"42"}));
    EXPECT_TRUE(!scalar_ok.has_value());
    EXPECT_TRUE(scalar.value.has_value());
    EXPECT_TRUE(scalar.value.value() == 42);
    auto scalar_err = scalar.into(make_parsed_arg("--count", {"not-a-number"}));
    EXPECT_TRUE(scalar_err.has_value());

    deco::decl::InputOption<int> input{};
    auto input_ok = input.into(make_parsed_arg("123"));
    EXPECT_TRUE(!input_ok.has_value());
    EXPECT_TRUE(input.value.has_value());
    EXPECT_TRUE(input.value.value() == 123);
    auto input_err = input.into(make_parsed_arg("bad-int"));
    EXPECT_TRUE(input_err.has_value());

    deco::decl::VectorOption<std::vector<int>> vector_opt{};
    auto vector_ok = vector_opt.into(make_parsed_arg("-P", {"7", "8"}));
    EXPECT_TRUE(!vector_ok.has_value());
    EXPECT_TRUE(vector_opt.value.has_value());
    EXPECT_TRUE(vector_opt.value.value().size() == 2);
    EXPECT_TRUE(vector_opt.value.value()[0] == 7);
    EXPECT_TRUE(vector_opt.value.value()[1] == 8);
    auto vector_err = vector_opt.into(make_parsed_arg("-P", {"7", "x"}));
    EXPECT_TRUE(vector_err.has_value());

    deco::decl::ScalarOption<CustomScalarResult> custom_scalar{};
    auto custom_scalar_ok = custom_scalar.into(make_parsed_arg("--name", {"alice"}));
    EXPECT_TRUE(!custom_scalar_ok.has_value());
    EXPECT_TRUE(custom_scalar.value.has_value());
    EXPECT_TRUE(custom_scalar.value->value == "alice");

    deco::decl::VectorOption<CustomVectorResult> custom_vector{};
    auto custom_vector_ok = custom_vector.into(make_parsed_arg("--tags", {"x", "y"}));
    EXPECT_TRUE(!custom_vector_ok.has_value());
    EXPECT_TRUE(custom_vector.value.has_value());
    EXPECT_TRUE(custom_vector.value->values.size() == 2);
    EXPECT_TRUE(custom_vector.value->values[0] == "x");
    EXPECT_TRUE(custom_vector.value->values[1] == "y");
}
}
;
