//===- OptTable.cpp - Option Table Implementation -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "opt_table.h"
#include "parsed_arg.h"
#include "opt_specifier.h"
#include "option.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <expected>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace catter::opt;

namespace {

std::string_view ltrim_all_of(std::string_view str, const std::vector<char>& prefixes) {
    auto pos = str.find_first_not_of(prefixes.data(), 0, prefixes.size());

    if(pos != std::string_view::npos) {
        return str.substr(pos, str.size());
    } else {
        return "";
    }
}

char safe_tolower(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

bool starts_with_insensitive(std::string_view text, std::string_view prefix) {
    if(prefix.size() > text.size())
        return false;

    for(size_t i = 0; i < prefix.size(); ++i) {
        if(safe_tolower(text[i]) != safe_tolower(prefix[i])) {
            return false;
        }
    }
    return true;
}

int compare_insensitive(std::string_view str1, std::string_view str2) {
    size_t mn = std::min(str1.size(), str2.size());
    for(size_t i = 0; i < mn; ++i) {
        auto c1 = safe_tolower(str1[i]);
        auto c2 = safe_tolower(str2[i]);

        if(c1 == c2) {
            continue;
        }
        return (c1 < c2) ? -1 : 1;
    }
    if(str1.size() == str2.size()) {
        return 0;
    }
    return str1.size() < str2.size() ? -1 : 1;
}

// Comparison function for Option strings (option names & prefixes).
// The ordering is *almost* case-insensitive lexicographic, with an exception.
// '\0' comes at the end of the alphabet instead of the beginning (thus options
// precede any other options which prefix them). Additionally, if two options
// are identical ignoring case, they are ordered according to case sensitive
// ordering if `FallbackCaseSensitive` is true.
int str_cmp_opt_name(std::string_view a, std::string_view b, bool fallback_case_sensitive) {
    size_t min_sz = std::min(a.size(), b.size());
    if(int res = compare_insensitive(a.substr(0, min_sz), (b.substr(0, min_sz)))) {
        return res;
    }

    // If they are identical ignoring case, use case sensitive ordering.
    if(a.size() == b.size())
        return fallback_case_sensitive ? a.compare(b) : 0;

    return (a.size() == min_sz) ? 1 /* A is a prefix of B. */
                                : -1 /* B is a prefix of A */;
}

struct OptNameLess {

    inline bool operator() (const OptTable::Info& i, std::string_view name) const {
        return str_cmp_opt_name(i.name(), name, false) < 0;
    }
};
}  // namespace

OptTable::OptTable(std::span<const OptTable::Info> option_infos,
                   bool ignore_case,
                   //    std::span<SubCommand> SubCommands,
                   std::vector<std::string_view> prefixes_union) :
    option_infos(option_infos), ignore_case(ignore_case), _prefixes_union(prefixes_union) {
    // Explicitly zero initialize the error to work around a bug in array
    // value-initialization on MinGW with gcc 4.3.5.

    // Find start of normal options.
    for(unsigned i = 0, e = this->num_options(); i != e; ++i) {
        unsigned kind = this->info(i + 1).kind;
        if(kind == Option::InputClass) {
            assert(!this->input_option_id && "Cannot have multiple input options!");
            this->input_option_id = this->info(i + 1).id;
        } else if(kind == Option::UnknownClass) {
            assert(!this->unknown_option_id && "Cannot have multiple unknown options!");
            this->unknown_option_id = this->info(i + 1).id;
        } else if(kind != Option::GroupClass) {
            this->first_searchable_index = i;
            break;
        }
    }
    assert(this->first_searchable_index != 0 && "No searchable options?");

    if(this->_prefixes_union.empty()) {
        std::set<std::string_view> tmp_prefixes_union;
        for(const auto& Info: option_infos.subspan(this->first_searchable_index)) {
            for(auto prefix: Info.prefixes()) {
                tmp_prefixes_union.insert(prefix);
            }
        }
        this->_prefixes_union =
            std::vector<std::string_view>(tmp_prefixes_union.begin(), tmp_prefixes_union.end());
    }

    buildPrefixChars();
}

const Option OptTable::option(OptSpecifier opt) const {
    unsigned id = opt.id();
    if(id == 0) {
        return Option(nullptr, nullptr);
    }
    assert((unsigned)(id - 1) < this->num_options() && "Invalid ID.");
    return Option(&this->info(id), this);
}

static bool is_input(const OptTable* o_table, std::string_view arg) {
    if(arg == "-") {
        return true;
    }
    assert(!o_table->prefixes_union().empty() && "Must have prefixes union.");
    for(const auto& prefix: o_table->prefixes_union()) {
        if(arg.starts_with(prefix)) {
            return false;
        }
    }
    return true;
}

/// \returns Matched size. 0 means no match.
static unsigned match_opt(const OptTable::Info* i, std::string_view str, bool ignore_case) {
    auto name = i->name();
    for(auto prefix: i->prefixes()) {
        if(str.starts_with(prefix)) {
            auto rest = str.substr(prefix.size());
            bool matched =
                ignore_case ? starts_with_insensitive(rest, name) : rest.starts_with(name);
            if(matched) {
                return prefix.size() + name.size();
            }
        }
    }
    return 0;
}

// Returns true if one of the Prefixes + In.Names matches Option
static bool opt_matches(const OptTable::Info& in, std::string_view option) {
    auto name = in.name();
    if(option.ends_with(name)) {
        option.remove_suffix(name.size());
        for(auto prefix: in.prefixes())
            if(option == prefix) {
                return true;
            }
    }
    return false;
}

// Parse a single argument, return the new argument, and update Index. If
// GroupedShortOptions is true, -a matches "-abc" and the argument in Args
// will be updated to "-bc". This overload does not support VisibilityMask
// or case insensitive options.
std::optional<ParsedArgument> OptTable::parse_one_arg_grouped(InputArgVec& argv,
                                                              unsigned& index) const {
    // Anything that doesn't start with PrefixesUnion is an input, as is '-'
    // itself.
    std::string_view str = argv[index];
    if(is_input(this, str)) {
        return ParsedArgument{
            .option_id = this->input_option_id,
            .spelling = str,
            .values = {},
            .index = index++,
        };
    }

    const Info* end = this->option_infos.data() + this->option_infos.size();
    auto name = ltrim_all_of(str, this->prefix_chars);
    const Info* start = std::lower_bound(this->option_infos.data() + this->first_searchable_index,
                                         end,
                                         name,
                                         OptNameLess());
    const Info* fallback_opt = nullptr;
    unsigned prev = index;

    // Search for the option which matches Str.
    for(; start != end; ++start) {
        unsigned arg_sz = match_opt(start, str, ignore_case);
        if(!arg_sz) {
            continue;
        }

        Option opt(start, this);
        if(auto a = opt.accept(argv,
                               std::string_view(argv[index]).substr(0, arg_sz),
                               /*GroupedShortOption=*/false,
                               index)) {
            return a;
        }

        // -abc, find -a, but -abc maybe a flag option, record -a as Fallback
        if(arg_sz == 2 && opt.kind() == Option::FlagClass) {
            fallback_opt = start;
        }

        // Otherwise, see if the argument is missing, index will be changed in accept.
        if(prev != index)
            return std::nullopt;
    }
    if(fallback_opt) {
        Option opt(fallback_opt, this);
        // Check that the last option isn't a flag wrongly given an argument.
        if(str[2] == '=') {
            return ParsedArgument{
                .option_id = this->unknown_option_id,
                .spelling = str,
                .values = {},
                .index = index++,
            };
        }

        if(auto a = opt.accept(argv, str.substr(0, 2), /*GroupedShortOption=*/true, index)) {
            argv[index] = '-' + std::string(str.substr(2));
            return a;
        }
    }

    // -abc, -a is invalid, to -bc
    if(str[1] != '-') {
        auto first_flag_name = str.substr(0, 2);
        auto r = ParsedArgument{
            .option_id = this->unknown_option_id,
            .spelling = to_spelling_array(first_flag_name),
            .values = {},
            .index = index,
        };
        argv[index] = '-' + std::string(str.substr(2));
        return r;
    }

    return ParsedArgument{
        .option_id = this->unknown_option_id,
        .spelling = str,
        .values = {},
        .index = index++,
    };
}

std::optional<ParsedArgument> OptTable::parse_one_arg(InputArgVec& argv,
                                                      unsigned& index,
                                                      Visibility visibility_mask) const {
    return internal_parse_one_arg(argv, index, [visibility_mask](const Option& Opt) {
        return !Opt.has_visibility_flag(visibility_mask);
    });
}

std::optional<ParsedArgument> OptTable::parse_one_arg(InputArgVec& argv,
                                                      unsigned& index,
                                                      unsigned flags_to_include,
                                                      unsigned flags_to_exclude) const {
    return internal_parse_one_arg(argv,
                                  index,
                                  [flags_to_include, flags_to_exclude](const Option& opt) {
                                      if(flags_to_include && !opt.has_flag(flags_to_include))
                                          return true;
                                      if(opt.has_flag(flags_to_exclude))
                                          return true;
                                      return false;
                                  });
}

std::optional<ParsedArgument>
    OptTable::internal_parse_one_arg(InputArgVec& argv,
                                     unsigned& index,
                                     std::function<bool(const Option&)> exclude_option) const {
    unsigned prev = index;
    auto str = std::string_view(argv[index]);

    // Anything that doesn't start with PrefixesUnion is an input, as is '-'
    // itself.
    if(is_input(this, str)) {
        return ParsedArgument{
            .option_id = this->input_option_id,
            .spelling = str,
            .values = {},
            .index = index++,
        };
    }

    const Info* start = this->option_infos.data() + this->first_searchable_index;
    const Info* end = this->option_infos.data() + this->option_infos.size();
    auto name = ltrim_all_of(str, this->prefix_chars);

    // Search for the first next option which could be a prefix.
    start = std::lower_bound(start, end, name, OptNameLess());

    // Options are stored in sorted order, with '\0' at the end of the
    // alphabet. Since the only options which can accept a string must
    // prefix it, we iteratively search for the next option which could
    // be a prefix.
    //
    // FIXME: This is searching much more than necessary, but I am
    // blanking on the simplest way to make it fast. We can solve this
    // problem when we move to TableGen.
    for(; start != end; ++start) {
        unsigned arg_sz = 0;
        // Scan for first option which is a proper prefix.
        for(; start != end; ++start) {
            if(arg_sz = match_opt(start, str, this->ignore_case); arg_sz) {
                break;
            }
        }
        if(start == end) {
            break;
        }

        Option opt(start, this);

        if(exclude_option(opt)) {
            continue;
        }

        // See if this option matches.
        if(auto a = opt.accept(argv,
                               std::string_view(argv[index]).substr(0, arg_sz),
                               /*GroupedShortOption=*/false,
                               index)) {
            return a;
        }

        // Otherwise, see if this argument was missing values.
        if(prev != index)
            return std::nullopt;
    }

    // If we failed to find an option and this arg started with /, then it's
    // probably an input path.
    if(str[0] == '/') {
        return ParsedArgument{
            .option_id = this->input_option_id,
            .spelling = str,
            .values = {},
            .index = index++,
        };
    }

    return ParsedArgument{
        .option_id = this->unknown_option_id,
        .spelling = str,
        .values = {},
        .index = index++,
    };
}

void OptTable::parse_args(InputArgVec& argv,
                          unsigned& missing_arg_index,
                          unsigned& missing_arg_count,
                          std::function<void(std::expected<ParsedArgument, std::string>)> fn,
                          Visibility visibility_mask) const {
    return internal_parse_args(
        argv,
        missing_arg_index,
        missing_arg_count,
        fn,
        [visibility_mask](const Option& opt) { return !opt.has_visibility_flag(visibility_mask); });
}

void OptTable::parse_args(InputArgVec& argv,
                          unsigned& missing_arg_index,
                          unsigned& missing_arg_count,
                          std::function<void(std::expected<ParsedArgument, std::string>)> fn,
                          unsigned flags_to_include,
                          unsigned flags_to_exclude) const {
    return internal_parse_args(argv,
                               missing_arg_index,
                               missing_arg_count,
                               fn,
                               [flags_to_include, flags_to_exclude](const Option& Opt) {
                                   if(flags_to_include && !Opt.has_flag(flags_to_include))
                                       return true;
                                   if(Opt.has_flag(flags_to_exclude))
                                       return true;
                                   return false;
                               });
}

void
    OptTable::parse_args(InputArgVec& argv,
                         std::function<void(std::expected<ParsedArgument, std::string>)> fn) const {

    unsigned MAI, MAC;
    parse_args(argv, MAI, MAC, fn);
    if(MAC) {
        fn(std::unexpected(argv[MAI] + ": missing argument"));
    }
}

void OptTable::internal_parse_args(
    InputArgVec& argv,
    unsigned& missing_arg_index,
    unsigned& missing_arg_count,
    std::function<void(std::expected<ParsedArgument, std::string>)> fn,
    std::function<bool(const Option&)> exclude_option) const {

    // FIXME: Handle '@' args (or at least error on them).

    missing_arg_index = missing_arg_count = 0;
    unsigned index = 0, end = argv.size();
    while(index < end) {
        // Ignore empty arguments (other things may still take them as
        // arguments).
        auto str = std::string_view(argv[index]);
        if(str.empty()) {
            ++index;
            continue;
        }

        // In DashDashParsing mode, the first "--" stops option scanning and
        // treats all subsequent arguments as positional.
        if(this->dash_dash_parsing && str == "--") {
            while(++index < end) {
                fn(ParsedArgument{
                    .option_id = this->input_option_id,
                    .spelling = std::string_view(argv[index]),
                    .values = {},
                    .index = index,
                });
            }
            break;
        }

        unsigned prev = index;
        auto a = this->grouped_short_options
                     ? this->parse_one_arg_grouped(argv, index)
                     : this->internal_parse_one_arg(argv, index, exclude_option);
        assert((index > prev || this->grouped_short_options) &&
               "Parser failed to consume argument.");

        // Check for missing argument error.
        if(!a) {
            assert(index >= end && "Unexpected parser error.");
            assert(index - prev - 1 && "No missing arguments!");
            missing_arg_index = prev;
            missing_arg_count = index - prev - 1;
            return;
        }
        fn(a.value());
    }
}
