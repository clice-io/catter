#pragma once

/* This file is for easily use the option of catter-proxy
 */

#include "opt-data/catter-proxy/table.h"
#include "util/option.h"
#include <expected>
#include <format>
#include <print>
#include <stdexcept>
#include <string>
#include <vector>

namespace catter::optdata::catter_proxy {

struct Option {
    std::string parent_id;
    std::string executable;
    std::string error_msg;
    std::vector<std::string> raw_argv;
};

inline Option parse_opt(std::span<std::string> argv_span, bool with_program_name = true) {
    Option option{};

    auto argv = with_program_name ? argv_span.subspan(1) : argv_span;
    if(argv.empty()) {
        throw std::invalid_argument("no arguments provided");
    }

    std::string error = "";
    catter_proxy_opt_table.parse_args(
        argv,
        [&](const std::expected<opt::ParsedArgument, std::string>& arg) {
            if(!arg.has_value()) {
                error = std::format("error parsing arguments: {}", arg.error());
                return;
            }

            switch(arg->option_id.id()) {
                case OPT_PARENT_ID: {
                    option.parent_id = arg->values[0];
                    break;
                }
                case OPT_EXEC: {
                    option.executable = arg->values[0];
                    break;
                }
                case OPT_INPUT: {
                    if(arg->get_spelling_view() == "--") {
                        option.raw_argv =
                            std::vector<std::string>(arg->values.begin(), arg->values.end());
                    } else {
                        option.error_msg = arg->get_spelling_view();
                    }
                    break;
                }
                default: {
                    error = std::format("unknown argument: {}", argv[arg->index]);
                    break;
                }
            }
        });
    if(!error.empty()) {
        throw std::invalid_argument(error);
    }
    return option;
};

inline Option parse_opt(int argc, char* argv[], bool with_program_name = true) {
    auto argv_vec = catter::util::save_argv(argc, argv);
    auto argv_span = std::span<std::string>(argv_vec);
    return parse_opt(argv_span, with_program_name);
};

};  // namespace catter::optdata::catter_proxy
