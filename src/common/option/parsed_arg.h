#pragma once

#include <array>
#include <cassert>
#include <ostream>
#include <span>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>
#include <optional>
#include "opt_specifier.h"
#include "util/meta.h"

namespace catter::opt {
struct OptionEnum {

    enum OptionClass {
        GroupClass = 0,
        InputClass,
        UnknownClass,
        FlagClass,
        JoinedClass,
        ValuesClass,
        SeparateClass,
        RemainingArgsClass,
        RemainingArgsJoinedClass,
        CommaJoinedClass,
        MultiArgClass,
        JoinedOrSeparateClass,
        JoinedAndSeparateClass
    };

    enum RenderStyleKind {
        RenderCommaJoinedStyle,
        RenderJoinedStyle,
        RenderSeparateStyle,
        RenderValuesStyle
    };
};

template <typename T>
concept StringOrVariant = std::is_convertible_v<T, std::string_view> ||
                          (std::is_same_v<T, std::variant<std::string_view, std::array<char, 8>>>);

template <typename SpellingTy, typename ValueTy>
struct ParsedArgumentBase {

    /// The unique identifier for the option, related to the index in option table.
    OptSpecifier option_id;
    /// the spelling of the argument, eg. "-I", "--optimize"
    /// Usually it is a string_view pointing to the original argv,
    /// but in some cases (eg. grouped short options) it may be a temporary string,
    /// therefore we should store it.
    SpellingTy spelling;
    /// the values associated with the argument, eg. -I/usr/include would have value "/usr/include"
    std::vector<ValueTy> values;
    /// the index of the argument in the original argv list.
    unsigned index;

    /// If this argument is an alias of another argument, this points to the original argument.
    /// eg. input an option "-O2", is an alias of "-O" with value "2".
    std::optional<OptSpecifier> unaliased_option_id = std::nullopt;

    /// Some alias also has additional values
    /// eg. "--optimize-size" is an alias of "-O" with additional value "s".
    std::optional<std::vector<ValueTy>> unaliased_addition_values = std::nullopt;

    /// Util helper
    std::string_view get_spelling_view() const {
        if constexpr(std::is_convertible_v<SpellingTy, std::string_view>) {
            return std::string_view(spelling);
        } else {
            return std::visit(
                [](auto&& arg) -> std::string_view {
                    if constexpr(std::is_convertible_v<std::decay_t<decltype(arg)>,
                                                       std::string_view>) {
                        return std::string_view(arg);
                    } else if constexpr(std::is_same_v<std::decay_t<decltype(arg)>,
                                                       std::array<char, 8>>) {
                        return std::string_view(arg.data());
                    } else {
                        static_assert(
                            !meta::dep_true<decltype(arg)>,
                            "Unhandled variant type in ParsedArgumentBase::get_spelling_view");
                        return "";
                    }
                },
                spelling);
        }
    }

    std::span<const ValueTy> original_values_ref() const {
        return std::span(this->values);
    }

    /// Convert to string for pass to a excutable
    /// It does not ensure that it is equal to the original argv, but it has the same meaning
    /// according to the table
    void to_arg_str(std::ostream& o, OptionEnum::RenderStyleKind render_kind) {
        switch(render_kind) {
            case OptionEnum::RenderCommaJoinedStyle: {
                o << this->get_spelling_view();
                for(const auto& val: this->values) {
                    o << ',' << val;
                }
            }; break;
            case OptionEnum::RenderJoinedStyle:
                assert(!this->values.empty() && "Joined option must have at least one value!");
                o << this->get_spelling_view() << this->values[0];
                break;
            case OptionEnum::RenderSeparateStyle: {
                o << this->get_spelling_view();
                for(const auto& val: this->values) {
                    o << ' ' << val;
                }
            }; break;
            case OptionEnum::RenderValuesStyle: {
                o << this->get_spelling_view();
            } break;
        }
    }

    OptSpecifier unaliased_opt() const {
        if(this->unaliased_option_id.has_value()) {
            return this->unaliased_option_id.value();
        }
        return this->option_id;
    }

    std::vector<ValueTy> unaliased_values() const {
        std::vector<ValueTy> vals = this->values;
        if(this->unaliased_addition_values.has_value()) {
            auto addition_vals = this->unaliased_addition_values.value();
            vals.insert(vals.end(), addition_vals.begin(), addition_vals.end());
            return vals;
        }
        return this->values;
    }

    std::vector<std::string_view> unaliased_values_view() const {
        std::vector<std::string_view> vals;
        for(const auto& v: this->values) {
            vals.emplace_back(std::string_view(v));
        }
        if(this->unaliased_addition_values.has_value()) {
            auto addition_vals = this->unaliased_addition_values.value();
            for(const auto& v: addition_vals) {
                vals.emplace_back(std::string_view(v));
            }
            return vals;
        }
        return vals;
    }
};

/**
 * Represents a parsed command-line argument.
 * You should get the meaning of a arg with a opt table.
 * The data depends on the option table and previously argv.
 */
using ParsedArgument =
    ParsedArgumentBase<std::variant<std::string_view, std::array<char, 8>>, std::string_view>;

struct ParsedArgumentOwning : public ParsedArgumentBase<std::string, std::string> {
    static ParsedArgumentOwning from_parsed_argument(const ParsedArgument& arg) {
        ParsedArgumentOwning owning_arg;
        owning_arg.option_id = arg.option_id;
        owning_arg.spelling = std::string(arg.get_spelling_view());
        for(const auto& val: arg.values) {
            owning_arg.values.emplace_back(std::string(val));
        }
        owning_arg.index = arg.index;
        owning_arg.unaliased_option_id = arg.unaliased_option_id;
        if(arg.unaliased_addition_values.has_value()) {
            std::vector<std::string> addition_vals;
            for(const auto& val: arg.unaliased_addition_values.value()) {
                addition_vals.emplace_back(std::string(val));
            }
            owning_arg.unaliased_addition_values = addition_vals;
        }
        return owning_arg;
    }
};

/// Helper to convert a string_view to an array for storage in the variant.
inline std::array<char, 8> to_spelling_array(std::string_view str) {
    std::array<char, 8> arr{};
    if(str.size() >= arr.size()) {
        // --, -, / cannot exceed the size, therefore it never happen if normal.
        str.copy(arr.data(), arr.size());
        arr[arr.size() - 1] = '\0';
    } else {
        str.copy(arr.data(), str.size());
        arr[str.size()] = '\0';
    }
    return arr;
};

}  // namespace catter::opt
