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
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <utility>
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

struct SubCommandError {
    enum class Type { Internal, MissingSubCommand, UnknownSubCommand };

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

        if(storage.is_unknown_option_id(arg->option_id)) {
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
    std::set<const decl::Category*> required_categories;
    // 0 is dummy
    for(std::size_t i = 1; i < c_map.size(); ++i) {
        const auto* category = c_map[i];
        if(category != nullptr && category->required) {
            required_categories.insert(category);
        }
    }
    for(const auto* category: required_categories) {
        if(!res.matched_categories.contains(category)) {
            err = {ParseError::Type::DecoParsing,
                   std::format("required {} is missing", desc::detail::category_desc(*category))};
            return std::unexpected(std::move(err));
        }
    }
    // check category exclusiveness
    for(auto category: res.matched_categories) {
        if(category->exclusive && res.matched_categories.size() > 1) {
            err = {ParseError::Type::DecoParsing,
                   std::format("options in {} are exclusive, but multiple categories are matched",
                               desc::detail::category_desc(*category))};
            return std::unexpected(std::move(err));
        }
    }
    return res;
}

template <typename T>
class Dispatcher {
    using handler_fn_t = std::move_only_function<void(T value)>;
    using error_fn_t = std::move_only_function<void(ParseError)>;

    handler_fn_t default_handler_ = [](auto) {
        return "nothing we can do with this options";
    };
    error_fn_t error_handler_ = [](auto err) {
        std::cerr << err.message << "\n";
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
        error_handler_ = [&os](const ParseError& err) {
            os << err.message << "\n";
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

    void parse(std::span<std::string> argv) {
        auto res = cli::parse<T>(argv);
        if(res.has_value()) {
            const auto& matched_categories = res->matched_categories;
            for(auto category: matched_categories) {
                auto it = handlers_.find(category);
                if(it != handlers_.end()) {
                    it->second(std::move(res->options));
                    return;
                }
            }
            default_handler_(std::move(res->options));
        } else {
            error_handler_(std::move(res.error()));
        }
    }

    void operator() (std::span<std::string> argv) {
        return parse(argv);
    }
};

class SubCommander {
    using handler_fn_t = std::move_only_function<void(std::span<std::string>)>;
    using error_fn_t = std::move_only_function<void(SubCommandError)>;

    struct SubCommandHandler {
        std::string name;
        std::string description;
        std::string command;
        handler_fn_t handler;
    };

    error_fn_t error_handler_ = [](const SubCommandError& err) {
        std::cerr << err.message << "\n";
    };
    std::optional<handler_fn_t> default_handler_;
    std::vector<SubCommandHandler> handlers_;
    std::map<std::string, std::size_t, std::less<>> command_to_handler_;

    std::string command_overview_;
    std::string overview_;

    static auto command_of(const decl::SubCommand& subcommand) -> std::string {
        if(subcommand.command.has_value()) {
            return std::string(*subcommand.command);
        }
        return std::string(subcommand.name);
    }

    static auto display_name_of(const decl::SubCommand& subcommand, std::string_view command)
        -> std::string {
        if(!subcommand.name.empty()) {
            return std::string(subcommand.name);
        }
        return std::string(command);
    }

public:
    SubCommander(std::string_view command_overview, std::string_view overview = {}) :
        command_overview_(command_overview), overview_(overview) {}

    auto& add(const decl::SubCommand& subcommand, handler_fn_t handler) {
        std::string command = command_of(subcommand);
        if(command.empty()) {
            error_handler_(
                {SubCommandError::Type::Internal, "subcommand name/command must not be empty"});
            return *this;
        }

        std::string name = display_name_of(subcommand, command);
        std::string description(subcommand.description);

        if(auto it = command_to_handler_.find(command); it != command_to_handler_.end()) {
            auto& target = handlers_[it->second];
            target.name = std::move(name);
            target.description = std::move(description);
            target.command = std::move(command);
            target.handler = std::move(handler);
            return *this;
        }

        command_to_handler_[command] = handlers_.size();
        handlers_.push_back({
            .name = std::move(name),
            .description = std::move(description),
            .command = std::move(command),
            .handler = std::move(handler),
        });
        return *this;
    }

    template <typename OptTy>
    auto& add(const decl::SubCommand& subcommand, Dispatcher<OptTy>& dispatcher) {
        return add(subcommand,
                   handler_fn_t([&dispatcher](std::span<std::string> argv) { dispatcher(argv); }));
    }

    template <typename OptTy>
    auto& add(const decl::SubCommand& subcommand, Dispatcher<OptTy>&& dispatcher) {
        return add(subcommand,
                   handler_fn_t([dispatcher = std::move(dispatcher)](
                                    std::span<std::string> argv) mutable { dispatcher(argv); }));
    }

    auto& add(handler_fn_t default_handler) {
        default_handler_ = std::move(default_handler);
        return *this;
    }

    auto& when_err(error_fn_t error_handler) {
        error_handler_ = std::move(error_handler);
        return *this;
    }

    auto& when_err(std::ostream& os) {
        error_handler_ = [&os](const SubCommandError& err) {
            os << err.message << "\n";
        };
        return *this;
    }

    template <typename Os>
    constexpr void usage(Os& os) const {
        if(!overview_.empty()) {
            os << overview_ << "\n\n";
        }

        if(default_handler_.has_value()) {
            os << "usage: " << command_overview_ << "\n";
            if(!handlers_.empty()) {
                os << "\n";
            }
        }

        if(handlers_.empty()) {
            return;
        }

        std::size_t max_name_len = 0;
        for(const auto& it: handlers_) {
            if(it.name.size() > max_name_len) {
                max_name_len = it.name.size();
            }
        }

        os << "Subcommands:\n";
        for(const auto& it: handlers_) {
            os << "  " << it.name;
            if(!it.description.empty()) {
                os << std::string(max_name_len - it.name.size() + 2, ' ') << it.description;
            }
            if(it.command != it.name) {
                os << " (" << it.command << ")";
            }
            os << "\n";
        }
    }

    void parse(std::span<std::string> argv) {
        if(!argv.empty()) {
            if(auto it = command_to_handler_.find(argv.front()); it != command_to_handler_.end()) {
                handlers_[it->second].handler(argv.subspan(1));
                return;
            }
        }

        if(default_handler_.has_value()) {
            (*default_handler_)(argv);
            return;
        }

        if(argv.empty()) {
            error_handler_({SubCommandError::Type::MissingSubCommand, "subcommand is required"});
            return;
        }

        error_handler_({SubCommandError::Type::UnknownSubCommand,
                        std::format("unknown subcommand '{}'", argv.front())});
    }

    void operator() (std::span<std::string> argv) {
        return parse(argv);
    }
};
};  // namespace deco::cli
