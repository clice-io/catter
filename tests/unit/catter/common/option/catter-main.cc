#include "util.h"
#include "util/output.h"
#include <boost/ut.hpp>
#include <opt-data/catter/table.h>
#include <string_view>
#include <vector>
using namespace boost;
using namespace catter;
using namespace std::literals::string_view_literals;

static ut::suite<"opt-catter-main"> ocp = [] {
    ut::test("option table has expected options") = [&] {
        auto argv =
            split2vec("-p 1234 -s script::profile --dest=114514 -- /usr/bin/clang++ --version");
        optdata::main::catter_proxy_opt_table.parse_args(
            argv,
            [](std::expected<opt::ParsedArgument, std::string> arg) {
                ut::expect(arg.has_value());
                switch(arg->option_id.id()) {
                    case optdata::main::OPT_HELP:
                        ut::expect(false) << "Did not expect help option.";
                        break;
                    case optdata::main::OPT_HELP_SHORT:
                        ut::expect(false) << "Did not expect help option.";
                        break;
                    case optdata::main::OPT_SCRIPT:
                        ut::expect(arg->values.size() == 1);
                        ut::expect(arg->values[0] == "script::profile");
                        ut::expect(arg->get_spelling_view() == "-s");
                        ut::expect(arg->index == 2);
                        break;
                    case optdata::main::OPT_UNKNOWN:
                        ut::expect(arg->get_spelling_view() == "--dest=114514" ||
                                   arg->get_spelling_view() == "-p");
                        break;
                    default: {
                        ut::expect(arg->option_id.id() == optdata::main::OPT_INPUT)
                            << arg->option_id.id();
                        if(arg->get_spelling_view() == "--") {
                            ut::expect(arg->index == 5);
                            ut::expect(arg->values.size() == 2);
                        } else {
                            ut::expect(arg->get_spelling_view() == "1234");
                        }
                    }
                }
            });

        argv = split2vec("-h");
        optdata::main::catter_proxy_opt_table.parse_args(
            argv,
            [](std::expected<opt::ParsedArgument, std::string> arg) {
                ut::expect(arg.has_value()) << arg.error();
                ut::expect(arg->option_id.id() == optdata::main::OPT_HELP_SHORT);
                ut::expect(arg->get_spelling_view() == "-h");
                ut::expect(arg->values.size() == 0);
                ut::expect(arg->index == 0);
                ut::expect(arg->unaliased_opt().id() == optdata::main::OPT_HELP);
            });
    };
};
