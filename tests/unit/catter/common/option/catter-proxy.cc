#include "util.h"
#include "util/output.h"
#include <boost/ut.hpp>
#include <opt-data/catter-proxy/table.h>
#include <string_view>
#include <vector>

using namespace boost;
using namespace catter;
using namespace std::literals::string_view_literals;

static ut::suite<"opt-catter-proxy"> ocp = [] {
    ut::test("option table has expected options") = [&] {
        auto argv = std::vector<std::string>{"-p", "1234"};
        optdata::catter_proxy::catter_proxy_opt_table.parse_args(
            argv,
            [](std::expected<opt::ParsedArgument, std::string> arg) {
                ut::expect(arg.has_value());
                if(arg.has_value()) {
                    ut::expect(arg->option_id.id() == optdata::catter_proxy::OPT_PARENT_ID);
                    ut::expect(arg->values.size() == 1);
                    ut::expect(arg->values[0] == "1234");
                    ut::expect(arg->get_spelling_view() == "-p");
                    ut::expect(arg->index == 0);
                }
            });
        argv = split2vec("--exec /bin/ls");
        optdata::catter_proxy::catter_proxy_opt_table.parse_args(
            argv,
            [](std::expected<opt::ParsedArgument, std::string> arg) {
                ut::expect(arg.has_value());
                ut::expect(arg->option_id.id() == optdata::catter_proxy::OPT_EXEC);
                ut::expect(arg->values.size() == 1);
                ut::expect(arg->values[0] == "/bin/ls");
                ut::expect(arg->get_spelling_view() == "--exec");
                ut::expect(arg->index == 0);
            });

        argv = split2vec("-p 12 --exec /usr/bin/clang++ -- clang++ --version");
        optdata::catter_proxy::catter_proxy_opt_table.parse_args(
            argv,
            [](std::expected<opt::ParsedArgument, std::string> arg) {
                ut::expect(arg.has_value());
                if(arg->option_id.id() == optdata::catter_proxy::OPT_PARENT_ID) {
                    ut::expect(arg->values.size() == 1);
                    ut::expect(arg->values[0] == "12");
                    ut::expect(arg->get_spelling_view() == "-p");
                    ut::expect(arg->index == 0);
                } else if(arg->option_id.id() == optdata::catter_proxy::OPT_EXEC) {
                    ut::expect(arg->values.size() == 1);
                    ut::expect(arg->values[0] == "/usr/bin/clang++");
                    ut::expect(arg->get_spelling_view() == "--exec");
                    ut::expect(arg->index == 2);
                } else {
                    ut::expect(arg->option_id.id() == optdata::catter_proxy::OPT_INPUT);
                    ut::expect(arg->get_spelling_view() == "clang++" ||
                               arg->get_spelling_view() == "--version");
                    ut::expect(arg->index == 5 || arg->index == 6);
                }
            });
    };
    ut::test("test unknown option") = [&] {
        auto argv = split2vec("--unknown-option value");
        optdata::catter_proxy::catter_proxy_opt_table.parse_args(
            argv,
            [](std::expected<opt::ParsedArgument, std::string> arg) {
                ut::expect(arg.has_value());
                if(arg->option_id.id() == optdata::catter_proxy::OPT_UNKNOWN) {
                    ut::expect(arg->option_id.id() == optdata::catter_proxy::OPT_UNKNOWN);
                    ut::expect(arg->get_spelling_view() == "--unknown-option");
                    ut::expect(arg->values.size() == 0);
                    ut::expect(arg->index == 0);
                } else {
                    ut::expect(arg->option_id.id() == optdata::catter_proxy::OPT_INPUT);
                    ut::expect(arg->get_spelling_view() == "value");
                    ut::expect(arg->values.size() == 0);
                    ut::expect(arg->index == 1);
                }
            });
    };
    ut::test("test missing value") = [&] {
        auto argv = split2vec("-p");
        optdata::catter_proxy::catter_proxy_opt_table.parse_args(
            argv,
            [](std::expected<opt::ParsedArgument, std::string> arg) {
                ut::expect(!arg.has_value());
                catter::output::blueLn("Test Error: {}", arg.error());
            });
    };
};
