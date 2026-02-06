#pragma once

/* This file is for easily use the option of catter-proxy
 */

#include "opt-data/catter-proxy/table.h"
#include "util/option.h"
#include <expected>
#include <format>
#include <print>
#include <string>
#include <filesystem>
#include <vector>

namespace catter::optdata::catter_proxy {
namespace fs = std::filesystem;

struct Option {
    std::string parent_id;
    fs::path executable;
    std::expected<std::vector<std::string>, std::string> raw_argv_or_err;
};

inline std::expected<Option, std::string> parse_opt(std::span<std::string> argv_span,
                                                    bool with_program_name = true) {
    std::string err = "";
    Option option{};
    auto argv = with_program_name ? argv_span.subspan(1) : argv_span;
    if(argv.empty()) {
        err = "no arguments provided";
        return std::unexpected(err);
    }
    catter_proxy_opt_table.parse_args(
        argv,
        [&](const std::expected<opt::ParsedArgument, std::string>& arg) {
            if(!err.empty()) {
                return;
            }
            if(!arg.has_value()) {
                err = arg.error();
                return;
            }
            switch(arg->option_id.id()) {
                case OPT_PARENT_ID: option.parent_id = arg->values[0]; break;
                case OPT_EXEC: option.executable = fs::path(arg->values[0]); break;
                case OPT_INPUT:
                    if(arg->get_spelling_view() == "--") {
                        option.raw_argv_or_err =
                            std::vector<std::string>(arg->values.begin(), arg->values.end());
                    } else {
                        option.raw_argv_or_err = std::unexpected(arg->get_spelling_view());
                    }
                    break;
                default: err = std::format("unknown arg {}", argv_span[arg->index]); break;
            }
        });
    if(err.empty()) {
        return option;
    } else {
        return std::unexpected(err);
    }
};

inline std::expected<Option, std::string> parse_opt(char* const argv[],
                                                    bool with_program_name = true) {
    auto argv_vec = catter::util::save_argv(argv);
    auto argv_span = std::span<std::string>(argv_vec);
    return parse_opt(argv_span, with_program_name);
};

};  // namespace catter::optdata::catter_proxy
