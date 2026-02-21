#include "deco/decl.h"
#include "deco/macro.h"
#include <string>
#include <vector>
#include <zest/zest.h>

static_assert(deco::decl::SingleResultType<bool>);
static_assert(deco::decl::SingleResultType<int>);
static_assert(deco::decl::SingleResultType<std::string>);
static_assert(!deco::decl::SingleResultType<std::vector<int>>);
static_assert(deco::decl::MultiResultType<std::vector<int>>);
static_assert(deco::decl::MultiResultType<std::vector<std::string>>);
static_assert(!deco::decl::MultiResultType<int>);

struct DeclOpt {
    DecoFlag(v, help = "flag"; required = false; prefix = deco::decl::Prefix::Dash;) verbose = true;
    DecoInput(help = "input")<int> input = 42;
    DecoPack(help = "pack")<std::vector<std::string>> pack = std::vector<std::string> { "a", "b" };
    DecoKVStyled(level, deco::decl::KVStyle::Joined, help = "joined-kv";)<int> joined = 7;
    DecoKV(path, help = "separate-kv";)<std::string> path = "entry.js";
    DecoComma(tags, help = "comma";
              alias = {"T"};)<std::vector<std::string>> tags = std::vector<std::string> {
        "x",
        "y"
    };
    DecoMulti(pair, 2, help = "multi"; alias = {"P"};)<std::vector<int>> pair = std::vector<int> {
        1,
        2
    };
};

TEST_SUITE(deco_decl){
    TEST_CASE(option_declaration_has_expected_shape_and_default_assignment){DeclOpt opt{};

EXPECT_TRUE(opt.verbose.name == "v");
EXPECT_TRUE(opt.verbose.required == false);
EXPECT_TRUE(opt.verbose.value == true);
opt.verbose = false;
EXPECT_TRUE(opt.verbose.value == false);

EXPECT_TRUE(opt.input.value.has_value());
EXPECT_TRUE(opt.input.value.value() == 42);
opt.input = 64;
EXPECT_TRUE(opt.input.value.value() == 64);

EXPECT_TRUE(opt.pack.value.has_value());
EXPECT_TRUE(opt.pack.value.value().size() == 2);
EXPECT_TRUE(opt.pack.value.value()[0] == "a");
EXPECT_TRUE(opt.pack.value.value()[1] == "b");
opt.pack = std::vector<std::string>{"tail"};
EXPECT_TRUE(opt.pack.value.value().size() == 1);
EXPECT_TRUE(opt.pack.value.value()[0] == "tail");

EXPECT_TRUE(opt.joined.style == deco::decl::KVStyle::Joined);
EXPECT_TRUE(opt.joined.value.has_value());
EXPECT_TRUE(opt.joined.value.value() == 7);
opt.joined = 11;
EXPECT_TRUE(opt.joined.value.value() == 11);

EXPECT_TRUE(opt.path.style == deco::decl::KVStyle::Separate);
EXPECT_TRUE(opt.path.value.has_value());
EXPECT_TRUE(opt.path.value.value() == "entry.js");
opt.path = std::string("run.js");
EXPECT_TRUE(opt.path.value.value() == "run.js");

EXPECT_TRUE(opt.tags.alias.size() == 1);
EXPECT_TRUE(opt.tags.alias[0] == "T");
EXPECT_TRUE(opt.tags.value.has_value());
EXPECT_TRUE(opt.tags.value.value().size() == 2);
EXPECT_TRUE(opt.tags.value.value()[0] == "x");
EXPECT_TRUE(opt.tags.value.value()[1] == "y");
opt.tags = std::vector<std::string>{"only"};
EXPECT_TRUE(opt.tags.value.value().size() == 1);
EXPECT_TRUE(opt.tags.value.value()[0] == "only");

EXPECT_TRUE(opt.pair.arg_num == 2);
EXPECT_TRUE(opt.pair.alias.size() == 1);
EXPECT_TRUE(opt.pair.alias[0] == "P");
EXPECT_TRUE(opt.pair.value.has_value());
EXPECT_TRUE(opt.pair.value.value().size() == 2);
EXPECT_TRUE(opt.pair.value.value()[0] == 1);
EXPECT_TRUE(opt.pair.value.value()[1] == 2);
opt.pair = std::vector<int>{9, 8};
EXPECT_TRUE(opt.pair.value.value().size() == 2);
EXPECT_TRUE(opt.pair.value.value()[0] == 9);
EXPECT_TRUE(opt.pair.value.value()[1] == 8);
}
}
;
