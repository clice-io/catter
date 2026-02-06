#include "util.h"
#include "util/output.h"
#include "opt-data/catter/table.h"

#include <zest/zest.h>

#include <string_view>
#include <vector>

using namespace catter;
using namespace std::literals::string_view_literals;

TEST_SUITE(opt_catter_main) {
    TEST_CASE(option_table_has_expected_options) {
        auto argv =
            split2vec("-p 1234 -s script::profile --dest=114514 -- /usr/bin/clang++ --version");
        optdata::main::catter_proxy_opt_table.parse_args(
            argv,
            [&](std::expected<opt::ParsedArgument, std::string> arg) {
                EXPECT_TRUE(arg.has_value());
                switch(arg->option_id.id()) {
                    case optdata::main::OPT_HELP:
                        EXPECT_TRUE(false);  //<< "Did not expect help option.";
                        break;
                    case optdata::main::OPT_HELP_SHORT:
                        EXPECT_TRUE(false);  //<< "Did not expect help option.";
                        break;
                    case optdata::main::OPT_SCRIPT:
                        EXPECT_TRUE(arg->values.size() == 1);
                        EXPECT_TRUE(arg->values[0] == "script::profile");
                        EXPECT_TRUE(arg->get_spelling_view() == "-s");
                        EXPECT_TRUE(arg->index == 2);
                        break;
                    case optdata::main::OPT_UNKNOWN:
                        EXPECT_TRUE(arg->get_spelling_view() == "--dest=114514" ||
                                    arg->get_spelling_view() == "-p");
                        break;
                    default: {
                        EXPECT_TRUE(arg->option_id.id() ==
                                    optdata::main::OPT_INPUT);  //<< arg->option_id.id();
                        if(arg->get_spelling_view() == "--") {
                            EXPECT_TRUE(arg->index == 5);
                            EXPECT_TRUE(arg->values.size() == 2);
                        } else {
                            EXPECT_TRUE(arg->get_spelling_view() == "1234");
                        }
                    }
                }
            });

        argv = split2vec("-h");
        optdata::main::catter_proxy_opt_table.parse_args(
            argv,
            [&](std::expected<opt::ParsedArgument, std::string> arg) {
                EXPECT_TRUE(arg.has_value());
                EXPECT_TRUE(arg->option_id.id() == optdata::main::OPT_HELP_SHORT);
                EXPECT_TRUE(arg->get_spelling_view() == "-h");
                EXPECT_TRUE(arg->values.size() == 0);
                EXPECT_TRUE(arg->index == 0);
                EXPECT_TRUE(arg->unaliased_opt().id() == optdata::main::OPT_HELP);
            });
    };
};
