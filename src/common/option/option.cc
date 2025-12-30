//===- Option.cpp - Abstract Driver Options -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#include "option.h"
#include "parsed_arg.h"
#include "opt_table.h"
#include <cassert>
#include <cstring>
#include <optional>
#include <ostream>
#include <ranges>
#include <string_view>
#include <utility>

namespace catter::opt {

Option::Option(const OptTable::Info* info, const OptTable* owner) : info(info), owner(owner) {
    // Multi-level aliases are not supported. This just simplifies option
    // tracking, it is not an inherent limitation.
    assert((!info || !this->alias().valid() || !this->alias().alias().valid()) &&
           "Multi-level aliases are not supported.");

    if(info && this->alias_args()) {
        assert(this->alias().valid() && "Only alias options can have alias args.");
        assert(this->kind() == FlagClass && "Only Flag aliases can have alias args.");
        assert(this->alias().kind() != FlagClass && "Cannot provide alias args to a flag option.");
    }
}

void Option::print(std::ostream& o, bool add_new_line) const {
    o << "<";
    switch(this->kind()) {
        // FIXME: Use reflection to print enum names.
#define P(N)                                                                                       \
    case N: o << #N; break
        P(GroupClass);
        P(InputClass);
        P(UnknownClass);
        P(FlagClass);
        P(JoinedClass);
        P(ValuesClass);
        P(SeparateClass);
        P(CommaJoinedClass);
        P(MultiArgClass);
        P(JoinedOrSeparateClass);
        P(JoinedAndSeparateClass);
        P(RemainingArgsClass);
        P(RemainingArgsJoinedClass);
#undef P
    }

    if(!this->info->has_no_prefix()) {
        o << " Prefixes:[";
        for(size_t i = 0, n = this->info->num_prefixes(); i != n; ++i)
            o << '"' << this->info->prefixes()[i] << (i == n - 1 ? "\"" : "\", ");
        o << ']';
    }

    o << " Name:\"" << this->name() << '"';

    const Option group = this->group();
    if(group.valid()) {
        o << " Group:";
        group.print(o, /*AddNewLine=*/false);
    }

    const Option als = this->alias();
    if(als.valid()) {
        o << " Alias:";
        als.print(o, /*AddNewLine=*/false);
    }

    if(this->kind() == MultiArgClass)
        o << " NumArgs:" << this->num_args();

    o << ">";
    if(add_new_line) {
        o << "\n";
    }
}

bool Option::matches(OptSpecifier opt) const {
    // Aliases are never considered in matching, look through them.
    const Option als = this->alias();
    if(als.valid()) {
        return als.matches(opt);
    }

    // Check exact match.
    if(this->id() == opt.id()) {
        return true;
    }

    const Option group = this->group();
    if(group.valid()) {
        return group.matches(opt);
    }
    return false;
}

std::optional<ParsedArgument> Option::accept_internal(const ArgList& args,
                                                      std::string_view spelling,
                                                      unsigned& index) const {
    const size_t spelling_sz = spelling.size();
    const size_t args_idx_sz = args[index].size();
    switch(this->kind()) {
        case FlagClass: {
            if(spelling_sz != args_idx_sz) {
                return std::nullopt;
            }
            return ParsedArgument{
                .option_id = this->id(),
                .spelling = spelling,
                .values = {},
                .index = index++,
            };
        }
        case JoinedClass: {
            auto value = std::string_view(args[index]).substr(spelling_sz);
            return ParsedArgument{
                .option_id = this->id(),
                .spelling = spelling,
                .values = {value},
                .index = index++,
            };
        }
        case CommaJoinedClass: {
            // Always matches.
            auto a = ParsedArgument{
                .option_id = this->id(),
                .spelling = spelling,
                .values = {},
                .index = index,
            };
            // Parse out the comma separated values.
            for(const auto& part:
                std::views::split(std::string_view(args[index]).substr(spelling_sz), ',') |
                    std::views::filter([](auto&& r) { return !r.empty(); })) {
                a.values.emplace_back(part);
            }
            index++;
            return a;
        }
        case SeparateClass:
            // Matches iff this is an exact match.
            if(spelling_sz != args_idx_sz) {
                return std::nullopt;
            }

            index += 2;
            if(index > args.size() || args[index - 1].empty()) {
                return std::nullopt;
            }

            return ParsedArgument{
                .option_id = this->id(),
                .spelling = spelling,
                .values = {std::string_view(args[index - 1])},
                .index = index - 2,
            };
        case MultiArgClass: {
            // Matches iff this is an exact match.
            if(spelling_sz != args_idx_sz) {
                return std::nullopt;
            }

            index += 1 + this->num_args();
            if(index > args.size())
                return std::nullopt;

            auto a = ParsedArgument{
                .option_id = this->id(),
                .spelling = spelling,
                .values = {},
                .index = index - (1 + this->num_args()),
            };
            for(unsigned i = 0; i != this->num_args(); ++i)
                a.values.emplace_back(std::string_view(args[index - this->num_args() + i]));
            return a;
        }
        case JoinedOrSeparateClass: {
            // If this is not an exact match, it is a joined arg.
            if(spelling_sz != args_idx_sz) {
                auto value = std::string_view(args[index]).substr(spelling_sz);
                return ParsedArgument{
                    .option_id = this->id(),
                    .spelling = spelling,
                    .values = {value},
                    .index = index++,
                };
            }

            // Otherwise it must be separate.
            index += 2;
            if(index > args.size() || args[index - 1].empty()) {
                return std::nullopt;
            }
            return ParsedArgument{
                .option_id = this->id(),
                .spelling = spelling,
                .values = {std::string_view(args[index - 1])},
                .index = index - 2,
            };
        }
        case JoinedAndSeparateClass:
            // Always matches.
            index += 2;
            if(index > args.size() || args[index - 1].empty()) {
                return std::nullopt;
            }
            return ParsedArgument{
                .option_id = this->id(),
                .spelling = spelling,
                .values = {std::string_view(args[index - 2]).substr(spelling_sz),
                           std::string_view(args[index - 1])},
                .index = index - 2,
            };
        case RemainingArgsClass: {
            // Matches iff this is an exact match.
            if(spelling_sz != args_idx_sz) {
                return std::nullopt;
            }
            auto a = ParsedArgument{
                .option_id = this->id(),
                .spelling = spelling,
                .values = {},
                .index = index++,
            };
            while(index < args.size() && !args[index].empty())
                a.values.push_back(std::string_view(args[index++]));
            return a;
        }
        case RemainingArgsJoinedClass: {
            auto a = ParsedArgument{
                .option_id = this->id(),
                .spelling = spelling,
                .values = {},
                .index = index,
            };
            if(spelling_sz != args_idx_sz) {
                // An inexact match means there is a joined arg.
                a.values.push_back(std::string_view(args[index]).substr(spelling_sz));
            }
            index++;
            while(index < args.size() && !args[index].empty())
                a.values.push_back(args[index++]);
            return a;
        }

        default: std::unreachable();
    }
}

std::optional<ParsedArgument> Option::accept(const ArgList& args,
                                             std::string_view spelling,
                                             bool grouped_short_option,
                                             unsigned& index) const {
    std::optional<ParsedArgument> a;
    if(grouped_short_option && this->kind() == FlagClass) {
        // when grouped short option, it is a temporary spelling from argv
        // because argv[index] will be changed to the remaining part after this option
        // therefore we should store the spelling into the variant

        a = ParsedArgument{
            .option_id = this->id(),
            .spelling = to_spelling_array(spelling),
            .values = {},
            .index = index,
        };
    } else {
        a = this->accept_internal(args, spelling, index);
    }
    if(!a) {
        return std::nullopt;
    }

    const Option& unaliased_opt = this->alias();
    if(this->id() == unaliased_opt.id()) {
        return a;
    }

    // "a" is an alias for a different flag. For most clients it's more convenient
    // if this function returns unaliased Args, so create an unaliased arg for
    // returning.

    // It's a bit weird that aliased and unaliased arg share one index, but
    // the index is mostly use as a memory optimization in render().
    // Due to this, ArgList::getArgString(A->getIndex()) will return the spelling
    // of the aliased arg always, while A->getSpelling() returns either the
    // unaliased or the aliased arg, depending on which Arg object it's called on.
    a->unaliased_option_id = unaliased_opt.id();

    if(this->kind() != FlagClass) {
        return a;
    }

    // FlagClass aliases can have AliasArgs<>; add those to the unaliased arg.
    // eg. -O => --optimize 2
    if(const char* val = this->alias_args()) {
        a->unaliased_addition_values = {};
        while(*val != '\0') {
            a->unaliased_addition_values->push_back(val);
            // Move past the '\0' to the next argument.
            val += std::strlen(val) + 1;
        }
    }
    if(this->owner->option(a->unaliased_option_id.value()).kind() == JoinedClass &&
       !this->alias_args()) {
        a->unaliased_addition_values = {};
        // A Flag alias for a Joined option must provide an argument.
        a->unaliased_addition_values->push_back("");
    }
    return a;
}
}  // namespace catter::opt
