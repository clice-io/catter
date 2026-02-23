#pragma once
#include "deco/decl.h"
#include "deco/ty.h"

#include <format>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

/*
 * generate string that describes the structure of options declared in deco::desc namespace, and
 * provide utilities to access the fields of the options by their corresponding option specifiers.
 */

namespace deco::desc {
namespace detail {
constexpr std::string_view k_default_help_text = "no provided";

constexpr inline bool has_help_text(std::string_view help_text) {
    return !help_text.empty() && help_text != k_default_help_text;
}

constexpr inline std::string category_desc(const decl::Category& category) {
    if(!category.name.empty() && !category.description.empty()) {
        return std::format("<{}> ({})", category.name, category.description);
    }
    if(!category.name.empty()) {
        return std::format("<{}>", category.name);
    }
    if(!category.description.empty()) {
        return std::string(category.description);
    }
    return std::string("<unnamed category>");
}

constexpr inline std::string field_desc(std::string_view field_name,
                                        const decl::CommonOptionFields& cfg) {
    if(has_help_text(cfg.help)) {
        return std::format("{} ({})", field_name, cfg.help);
    }
    const auto category = cfg.category.ptr();
    if(!category->name.empty() || !category->description.empty()) {
        return std::format("{} [category: {}]", field_name, category_desc(*category));
    }
    return std::string(field_name);
}

constexpr inline std::string meta_var_token(std::string_view meta_var) {
    if(meta_var.empty()) {
        return "<value>";
    }
    if(meta_var.front() == '<' && meta_var.back() == '>') {
        return std::string(meta_var);
    }
    return std::format("<{}>", meta_var);
}

constexpr inline std::string join_strings(const std::vector<std::string>& parts,
                                          std::string_view separator) {
    if(parts.empty()) {
        return "";
    }
    std::string joined = parts.front();
    for(std::size_t i = 1; i < parts.size(); ++i) {
        joined += separator;
        joined += parts[i];
    }
    return joined;
}

constexpr inline std::string placeholder_name(decl::DecoType deco_field_ty) {
    switch(deco_field_ty) {
        case decl::DecoType::Flag: return "--<flag>";
        case decl::DecoType::KV: return "--<option>";
        case decl::DecoType::CommaJoined: return "--<list-option>";
        case decl::DecoType::Multi: return "--<multi-option>";
        default: return "<option>";
    }
}

constexpr inline std::string normalize_member_name(std::string_view member_name) {
    std::string normalized(member_name);
    for(auto& ch: normalized) {
        if(ch == '_') {
            ch = '-';
        }
    }
    return normalized;
}

constexpr inline std::string default_name_from_member(std::string_view member_name) {
    const auto normalized_name = normalize_member_name(member_name);
    if(normalized_name.empty()) {
        return "";
    }
    if(normalized_name.size() == 1) {
        return std::format("-{}", normalized_name);
    }
    return std::format("--{}", normalized_name);
}

template <typename CfgTy>
constexpr inline std::vector<std::string> named_aliases(const CfgTy& cfg,
                                                        std::string_view fallback_name) {
    std::vector<std::string> aliases;
    if constexpr(std::is_base_of_v<decl::NamedOptionFields, CfgTy>) {
        aliases.reserve(cfg.names.size());
        for(auto name: cfg.names) {
            aliases.emplace_back(name);
        }
    }
    if(aliases.empty()) {
        const auto generated_name = default_name_from_member(fallback_name);
        if(!generated_name.empty()) {
            aliases.push_back(generated_name);
        } else {
            aliases.emplace_back(placeholder_name(CfgTy::deco_field_ty));
        }
    }
    return aliases;
}

constexpr inline std::string join_aliases(const std::vector<std::string>& aliases, bool help_mode) {
    return join_strings(aliases, help_mode ? ", " : "|");
}

constexpr inline std::string kv_joined_alias(std::string_view alias, std::string_view value_token) {
    if(alias.starts_with("--")) {
        return std::format("{}={}", alias, value_token);
    }
    if(alias.starts_with("/")) {
        return std::format("{}:{}", alias, value_token);
    }
    return std::format("{}{}", alias, value_token);
}

constexpr inline std::string comma_joined_alias(std::string_view alias,
                                                std::string_view value_token) {
    return std::format("{},{}[,{}...]", alias, value_token, value_token);
}

constexpr inline std::string base_meta_name(std::string_view value_token) {
    if(value_token.size() >= 2 && value_token.front() == '<' && value_token.back() == '>') {
        return std::string(value_token.substr(1, value_token.size() - 2));
    }
    return std::string(value_token);
}

constexpr inline std::string repeated_meta_vars(std::string_view value_token, unsigned arg_num) {
    if(arg_num <= 1) {
        return std::string(value_token);
    }

    const auto base_name = base_meta_name(value_token);
    std::vector<std::string> values;
    values.reserve(arg_num);
    for(unsigned i = 1; i <= arg_num; ++i) {
        values.push_back(std::format("<{}{}>", base_name, i));
    }
    return join_strings(values, " ");
}

template <typename CfgTy>
constexpr inline std::string usage_text(const CfgTy& cfg,
                                        bool help_mode,
                                        std::string_view fallback_name) {
    const auto value_token = meta_var_token(cfg.meta_var);

    if constexpr(CfgTy::deco_field_ty == decl::DecoType::Input) {
        return value_token;
    } else if constexpr(CfgTy::deco_field_ty == decl::DecoType::TrailingInput) {
        return std::format("-- {}...", value_token);
    } else if constexpr(CfgTy::deco_field_ty == decl::DecoType::Flag) {
        return join_aliases(named_aliases(cfg, fallback_name), help_mode);
    } else if constexpr(CfgTy::deco_field_ty == decl::DecoType::KV) {
        const auto aliases = named_aliases(cfg, fallback_name);
        if(cfg.style == decl::KVStyle::Joined) {
            std::vector<std::string> forms;
            forms.reserve(aliases.size());
            for(const auto& alias: aliases) {
                forms.push_back(kv_joined_alias(alias, value_token));
            }
            return join_aliases(forms, help_mode);
        }
        return std::format("{} {}", join_aliases(aliases, help_mode), value_token);
    } else if constexpr(CfgTy::deco_field_ty == decl::DecoType::CommaJoined) {
        const auto aliases = named_aliases(cfg, fallback_name);
        std::vector<std::string> forms;
        forms.reserve(aliases.size());
        for(const auto& alias: aliases) {
            forms.push_back(comma_joined_alias(alias, value_token));
        }
        return join_aliases(forms, help_mode);
    } else if constexpr(CfgTy::deco_field_ty == decl::DecoType::Multi) {
        return std::format("{} {}",
                           join_aliases(named_aliases(cfg, fallback_name), help_mode),
                           repeated_meta_vars(value_token, cfg.arg_num));
    } else {
        return "<unsupported>";
    }
}

template <typename CfgTy>
constexpr inline std::string help_text(const CfgTy& cfg, std::string_view fallback_name) {
    const auto usage = usage_text(cfg, true, fallback_name);
    constexpr size_t usage_column_width = 32;
    if(usage.size() >= usage_column_width) {
        return std::format(R"(  {}
    {:<32}{})",
                           usage,
                           "",
                           has_help_text(cfg.help) ? cfg.help : "no description provided");
    }
    return std::format("  {:<32}{}",
                       usage,
                       has_help_text(cfg.help) ? cfg.help : "no description provided");
}
}  // namespace detail

template <ty::is_option_field T>
constexpr inline std::string from_deco_option(const T& field,
                                              bool include_help = false,
                                              std::string_view fallback_name = {}) {
    const auto cfg = ty::dyn_cast(field);
    if(include_help) {
        return detail::help_text(cfg, fallback_name);
    }
    return detail::usage_text(cfg, false, fallback_name);
}
}  // namespace deco::desc
