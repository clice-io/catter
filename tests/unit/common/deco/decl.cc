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

struct DeclOpt {
    DecoFlag({
        help = "flag";
        required = false;
        exclusive = true;
        category = 7;
    }) verbose = true;

    DECO_CFG(required = true);
    DecoInput(help = "input")<int> input = 42;

    DecoPack(help = "pack";
             category = 1;)<std::vector<std::string>> pack = std::vector<std::string> {
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

TEST_SUITE(deco_decl){
    TEST_CASE(option_declaration_has_expected_shape_and_default_assignment){DeclOpt opt{};

EXPECT_TRUE(opt.verbose.names.empty());
EXPECT_TRUE(opt.verbose.required == false);
EXPECT_TRUE(opt.verbose.exclusive == true);
EXPECT_TRUE(opt.verbose.category == 7);
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
EXPECT_TRUE(opt.pack.category == 1);
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

EXPECT_TRUE(opt.tags.names.size() == 1);
EXPECT_TRUE(opt.tags.names[0] == "-T");
EXPECT_TRUE(opt.tags.value.has_value());
EXPECT_TRUE(opt.tags.value.value().size() == 2);
EXPECT_TRUE(opt.tags.value.value()[0] == "x");
EXPECT_TRUE(opt.tags.value.value()[1] == "y");
opt.tags = std::vector<std::string>{"only"};
EXPECT_TRUE(opt.tags.value.value().size() == 1);
EXPECT_TRUE(opt.tags.value.value()[0] == "only");

EXPECT_TRUE(opt.pair.arg_num == 2);
EXPECT_TRUE(opt.pair.names.size() == 1);
EXPECT_TRUE(opt.pair.names[0] == "-P");
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
