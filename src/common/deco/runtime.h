#pragma once

#include "deco/backend.h"
#include "deco/decl.h"
#include "deco/descriptor.h"
#include "option/opt_table.h"
#include <cstddef>
#include <expected>
#include <format>
#include <functional>
#include <iostream>
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace deco::util {
inline std::vector<std::string> argvify(int argc, const char* const* argv, unsigned skip_num = 1) {
    std::vector<std::string> res;
    for(unsigned i = skip_num; i < static_cast<unsigned>(argc); ++i) {
        res.emplace_back(argv[i]);
    }
    return res;
}
}  // namespace deco::util

namespace deco::cli {

template <typename T>
struct ParsedResult {
    T options;
    std::set<const decl::Category*> matched_categories;
};

struct ParseError {
    enum class Type { Internal, BackendParsing, DecoParsing, IntoError };

    Type type;

    std::string message;
};

template <typename T>
std::expected<ParsedResult<T>, ParseError> parse(std::span<std::string> argv) {
    auto& storage = detail::build_storage<T>();
    backend::OptTable table = storage.make_opt_table();
    ParsedResult<T> res{};
    ParseError err;
    table.parse_args(argv, [&](std::expected<backend::ParsedArgument, std::string> arg) {
        if(!arg.has_value()) {
            err = {ParseError::Type::BackendParsing, std::move(arg.error())};
            return false;
        }

        if(arg->option_id == storage.unknown_option_id) {
            err = {ParseError::Type::BackendParsing,
                   std::format("unknown option '{}'", arg->get_spelling_view())};
            return false;
        }

        auto& raw_parg = *arg;
        const bool is_trailing_arg = storage.is_trailing_argument(raw_parg);

        void* opt_raw_ptr = is_trailing_arg ? storage.trailing_ptr_of(res.options)
                                            : storage.field_ptr_of(raw_parg.option_id, res.options);
        decl::DecoOptionBase* opt_accessor = static_cast<decl::DecoOptionBase*>(opt_raw_ptr);
        if(opt_accessor == nullptr) {
            if(is_trailing_arg) {
                err = {ParseError::Type::Internal,
                       "no option accessor found for trailing '--' option"};
            } else {
                err = {ParseError::Type::Internal,
                       "no option accessor found for option id " +
                           std::to_string(raw_parg.option_id.id())};
            }
            return false;
        }
        if(auto parse_err = opt_accessor->into(std::move(raw_parg))) {
            err = {ParseError::Type::IntoError, std::move(*parse_err)};
            return false;
        }

        const decl::Category* category =
            is_trailing_arg ? storage.trailing_category() : storage.category_of(raw_parg.option_id);
        if(category == nullptr) {
            if(is_trailing_arg) {
                err = {ParseError::Type::Internal, "no category found for trailing '--' option"};
            } else {
                err = {ParseError::Type::Internal,
                       "no category found for option id " +
                           std::to_string(raw_parg.option_id.id())};
            }
            return false;
        }
        res.matched_categories.insert(category);
        return true;
    });
    if(!err.message.empty()) {
        return std::unexpected(std::move(err));
    }
    // check required options
    storage.visit_fields(res.options,
                         [&](auto& field, const auto& cfg, std::string_view name, auto) {
                             if(res.matched_categories.contains(cfg.category.ptr()) &&
                                cfg.required && !field.value.has_value()) {
                                 err = {ParseError::Type::DecoParsing,
                                        std::format("required option {} is missing",
                                                    desc::from_deco_option(cfg, false, name))};
                                 return false;
                             }
                             return true;
                         });
    if(!err.message.empty()) {
        return std::unexpected(std::move(err));
    }

    // check category requirements
    const auto& c_map = storage.category_map();
    for(std::size_t i = 0; i < c_map.size() && c_map[i] != nullptr; ++i) {
        const auto* category = c_map[i];
        if(category->required && !res.matched_categories.contains(category)) {
            err = {ParseError::Type::DecoParsing,
                   std::format("required {} is missing", desc::detail::category_desc(*category))};
            return std::unexpected(std::move(err));
        }
    }
    // check category exclusiveness
    for(auto category: res.matched_categories) {
        if(category->exclusive && res.matched_categories.size() > 1) {
            err = {ParseError::Type::DecoParsing,
                   std::format("options in  are exclusive, but multiple categories are matched",
                               desc::detail::category_desc(*category))};
            return std::unexpected(std::move(err));
        }
    }
    return res;
}

template <typename T>
class Dispatcher {
    using handler_fn_t = std::move_only_function<void(const decl::Category*)>;
    using error_fn_t = std::move_only_function<void(std::string_view)>;

    handler_fn_t default_handler_ = [](auto) {
        return "nothing we can do with this options";
    };
    error_fn_t error_handler_ = [](std::string_view err) {
        std::cerr << err << "\n";
    };
    std::map<const deco::decl::Category*, handler_fn_t> handlers_;
    std::string_view command_overview_;

public:
    Dispatcher(std::string_view command_overview) : command_overview_(command_overview) {}

    auto& dispatch(const decl::Category& category, handler_fn_t handler) {
        handlers_[&category] = std::move(handler);
        return *this;
    }

    auto& dispatch(handler_fn_t handler) {
        default_handler_ = std::move(handler);
        return *this;
    }

    auto& when_err(error_fn_t error_handler) {
        error_handler_ = std::move(error_handler);
        return *this;
    }

    auto& when_err(std::ostream& os) {
        error_handler_ = [&os](std::string_view err) {
            os << err << "\n";
        };
        return *this;
    }

    template <typename Os>
    constexpr void usage(Os& os, bool include_help = true) const {
        constexpr auto& storage = detail::build_storage<T>();
        std::map<const decl::Category*, std::vector<std::string>> category_usage_map;
        auto on_option = [&](auto, const auto& opt_fields, std::string_view field_name, auto) {
            category_usage_map[opt_fields.category.ptr()].push_back(
                desc::from_deco_option(opt_fields, include_help, field_name));
            return true;
        };
        storage.visit_fields(T{}, on_option);
        os << "usage: " << command_overview_ << "\n\n";
        os << "Options:\n";
        if(category_usage_map.size() == 1 &&
           category_usage_map.begin()->first == &decl::default_category) {
            for(const auto& usage: category_usage_map.begin()->second) {
                os << "  " << usage << "\n";
            }
        } else {
            for(const auto& [category, usages]: category_usage_map) {
                os << "Group" << desc::detail::category_desc(*category);
                if(category->exclusive) {
                    os << ", exclusive with other groups";
                }
                os << ":\n";
                for(const auto& usage: usages) {
                    os << "  " << usage << "\n";
                }
                os << "\n";
            }
        }
    }
};
};  // namespace deco::cli
