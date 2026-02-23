#include "deco/descriptor.h"
#include "deco/macro.h"

#include <string>
#include <vector>
#include <zest/zest.h>

namespace {
struct DescOpt {
    DecoFlag(names = {"-v", "--verbose"}; help = "Show version and exit";
             required = false;) verbose;

    DecoKV(names = {"-o", "--output"}; meta_var = "FILE"; help = "Write output to FILE";
           required = false;)<std::string> output;

    DecoKVStyled(deco::decl::KVStyle::Joined, names = {"-I", "--include"}; meta_var = "DIR";
                 help = "Add include search path";
                 required = false;)<std::string> include_dir;

    DecoComma(names = {"--tags", "-T"}; meta_var = "TAG"; help = "Comma-separated tags";
              required = false;)<std::vector<std::string>> tags;

    DecoMulti(2, names = {"--pair"}; meta_var = "VAL"; help = "Two values";
              required = false;)<std::vector<std::string>> pair;

    DecoInput(meta_var = "INPUT"; help = "Input file"; required = false;)<std::string> input;
    DecoPack(meta_var = "ARG"; help = "Trailing arguments";
             required = false;)<std::vector<std::string>> trailing;

    DecoFlag(help = "Unnamed flag fallback"; required = false;) unnamed;
};
}  // namespace

TEST_SUITE(deco_descriptor){TEST_CASE(from_deco_option_renders_usage_style_text){DescOpt opt{};

EXPECT_TRUE(deco::desc::from_deco_option(opt.verbose) == "-v|--verbose");
EXPECT_TRUE(deco::desc::from_deco_option(opt.output) == "-o|--output <FILE>");
EXPECT_TRUE(deco::desc::from_deco_option(opt.include_dir) == "-I<DIR>|--include=<DIR>");
EXPECT_TRUE(deco::desc::from_deco_option(opt.tags) ==
            "--tags,<TAG>[,<TAG>...]|-T,<TAG>[,<TAG>...]");
EXPECT_TRUE(deco::desc::from_deco_option(opt.pair) == "--pair <VAL1> <VAL2>");
EXPECT_TRUE(deco::desc::from_deco_option(opt.input) == "<INPUT>");
EXPECT_TRUE(deco::desc::from_deco_option(opt.trailing) == "-- <ARG>...");
EXPECT_TRUE(deco::desc::from_deco_option(opt.unnamed) == "--<flag>");
EXPECT_TRUE(deco::desc::from_deco_option(opt.unnamed, false, "u") == "-u");
EXPECT_TRUE(deco::desc::from_deco_option(opt.unnamed, false, "long_name") == "--long-name");
}

TEST_CASE(from_deco_option_renders_help_style_text) {
    DescOpt opt{};

    const auto verbose_help = deco::desc::from_deco_option(opt.verbose, true);
    EXPECT_TRUE(verbose_help.find("-v, --verbose") != std::string::npos);
    EXPECT_TRUE(verbose_help.find("Show version and exit") != std::string::npos);

    const auto output_help = deco::desc::from_deco_option(opt.output, true);
    EXPECT_TRUE(output_help.find("-o, --output <FILE>") != std::string::npos);
    EXPECT_TRUE(output_help.find("Write output to FILE") != std::string::npos);

    const auto input_help = deco::desc::from_deco_option(opt.input, true);
    EXPECT_TRUE(input_help.find("<INPUT>") != std::string::npos);
    EXPECT_TRUE(input_help.find("Input file") != std::string::npos);
}
}
;
