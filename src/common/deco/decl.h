#pragma once
#include "option/parsed_arg.h"
#include "trait.h"
#include <charconv>
#include <cerrno>
#include <concepts>
#include <cstdlib>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace deco::decl {
enum class DecoType {
    Input,
    // after "--"
    TrailingInput,
    // -p
    Flag,
    // -o 1
    KV,
    // -x,a,b,c
    CommaJoined,
    // -x 1 2 3, fixed size
    Multi,
};

enum class KVStyle : char {
    // -KEYValue
    Joined = 0,
    // -o 1
    Separate = 1
};

struct Category {
    // If true, any other categories should not occur if this category occurs  in args.
    bool exclusive = false;
    // if true, this category is required in args.
    bool required = false;
    std::string_view name;
    std::string_view description;
};

constexpr inline Category default_category = {
    .exclusive = false,
    .required = true,
    .name = "default",
    .description = "the default category for options",
};

struct CategoryRef {
    const Category* value = &default_category;

    constexpr CategoryRef() = default;

    constexpr CategoryRef(const Category& category) : value(&category) {}

    constexpr auto operator= (const Category& category) -> CategoryRef& {
        value = &category;
        return *this;
    }

    constexpr auto ptr() const -> const Category* {
        return value;
    }

    constexpr auto get() const -> const Category& {
        return *value;
    }

    constexpr auto operator* () const -> const Category& {
        return *value;
    }

    constexpr auto operator->() const -> const Category* {
        return value;
    }

    constexpr operator const Category&() const {
        return *value;
    }
};

struct DecoFields {
    // if true, it's required if its category occurs in options.
    bool required = true;
    // options in the same category (same Category object address) must be all set or all unset
    CategoryRef category = default_category;
};

struct CommonOptionFields : DecoFields {
    std::string_view help = "no provided";
    std::string_view meta_var = "<value>";

    constexpr CommonOptionFields() = default;
};

template <typename Ty>
struct ConfigOverrideField {
    Ty value_{};
    bool overridden_ = false;

    constexpr ConfigOverrideField() = default;

    constexpr ConfigOverrideField(const Ty& value) : value_(value) {}

    template <typename U>
        requires std::constructible_from<Ty, U>
    constexpr ConfigOverrideField(U&& value) : value_(Ty(std::forward<U>(value))) {}

    template <typename U>
        requires std::constructible_from<Ty, U>
    constexpr auto operator= (U&& value) -> ConfigOverrideField& {
        value_ = Ty(std::forward<U>(value));
        overridden_ = true;
        return *this;
    }

    constexpr auto is_overridden() const -> bool {
        return overridden_;
    }

    constexpr auto get() const -> const Ty& {
        return value_;
    }
};

// just to override the default value in this area
struct ConfigFields : CommonOptionFields {
    ConfigOverrideField<bool> required = true;
    ConfigOverrideField<CategoryRef> category = default_category;
    ConfigOverrideField<std::string_view> help = "no provided";
    ConfigOverrideField<std::string_view> meta_var = "<value>";

    enum class Type : char {
        Start = 0,
        End = 1,
        Next = 2,  // just make sense to next
    };
    Type type;
};

struct NamedOptionFields : CommonOptionFields {
    std::vector<std::string_view> names;
    constexpr NamedOptionFields() = default;
};

struct InputFields : CommonOptionFields {
    constexpr static DecoType deco_field_ty = DecoType::Input;
};

struct PackFields : CommonOptionFields {
    constexpr static DecoType deco_field_ty = DecoType::TrailingInput;
};

struct FlagFields : NamedOptionFields {
    constexpr static DecoType deco_field_ty = DecoType::Flag;
};

struct KVFields : NamedOptionFields {
    constexpr static DecoType deco_field_ty = DecoType::KV;
    KVStyle style = KVStyle::Separate;
};

struct CommaJoinedFields : NamedOptionFields {
    constexpr static DecoType deco_field_ty = DecoType::CommaJoined;
};

struct MultiFields : NamedOptionFields {
    constexpr static DecoType deco_field_ty = DecoType::Multi;
    unsigned arg_num = 1;
};

struct DecoOptionBase {
    virtual ~DecoOptionBase() = default;
    // return error message if parsing fails, otherwise return std::nullopt
    virtual std::optional<std::string> into(backend::ParsedArgument&& arg) = 0;
};

template <typename ResTy>
struct DecoOption : DecoOptionBase {
    using result_type = ResTy;

    std::optional<ResTy> value = std::nullopt;

    constexpr DecoOption(ResTy default_value) : value(std::move(default_value)) {}

    template <typename DefaultTy>
        requires (!std::same_as<std::remove_cvref_t<DefaultTy>, DecoOption<ResTy>> &&
                  std::constructible_from<ResTy, DefaultTy>)
    constexpr DecoOption(DefaultTy&& default_value) :
        value(ResTy(std::forward<DefaultTy>(default_value))) {}

    constexpr DecoOption() = default;

    template <typename DefaultTy>
        requires (!std::same_as<std::remove_cvref_t<DefaultTy>, DecoOption<ResTy>> &&
                  std::constructible_from<ResTy, DefaultTy>)
    constexpr auto operator= (DefaultTy&& default_value) -> DecoOption<ResTy>& {
        value = ResTy(std::forward<DefaultTy>(default_value));
        return *this;
    }

    ResTy& operator* () {
        return *value;
    }

    const ResTy& operator* () const {
        return *value;
    }

    ResTy* operator->() {
        return &(*value);
    }

    const ResTy* operator->() const {
        return &(*value);
    }
};

namespace detail {
inline bool iequals_ascii(std::string_view lhs, std::string_view rhs) {
    if(lhs.size() != rhs.size()) {
        return false;
    }
    for(std::size_t i = 0; i < lhs.size(); ++i) {
        auto l = lhs[i];
        auto r = rhs[i];
        if(l >= 'A' && l <= 'Z') {
            l = static_cast<char>('a' + (l - 'A'));
        }
        if(r >= 'A' && r <= 'Z') {
            r = static_cast<char>('a' + (r - 'A'));
        }
        if(l != r) {
            return false;
        }
    }
    return true;
}

template <typename ErrorTy>
std::optional<std::string> normalize_into_error(const std::optional<ErrorTy>& err) {
    if(!err.has_value()) {
        return std::nullopt;
    }
    return std::string(std::string_view(*err));
}

template <typename ResTy>
std::optional<std::string> parse_primitive_scalar(ResTy& out, std::string_view text) {
    if constexpr(std::same_as<ResTy, bool>) {
        if(text == "1" || iequals_ascii(text, "true") || iequals_ascii(text, "yes") ||
           iequals_ascii(text, "on")) {
            out = true;
            return std::nullopt;
        }
        if(text == "0" || iequals_ascii(text, "false") || iequals_ascii(text, "no") ||
           iequals_ascii(text, "off")) {
            out = false;
            return std::nullopt;
        }
        return "invalid boolean value: " + std::string(text);
    } else if constexpr(std::integral<ResTy>) {
        ResTy parsed{};
        const auto* begin = text.data();
        const auto* end = text.data() + text.size();
        const auto [ptr, ec] = std::from_chars(begin, end, parsed);
        if(ec != std::errc() || ptr != end) {
            return "invalid integer value: " + std::string(text);
        }
        out = parsed;
        return std::nullopt;
    } else if constexpr(std::floating_point<ResTy>) {
        std::string copy(text);
        char* parse_end = nullptr;
        errno = 0;
        const auto parsed = std::strtold(copy.c_str(), &parse_end);
        if(parse_end != copy.c_str() + copy.size() || errno == ERANGE) {
            return "invalid floating-point value: " + std::string(text);
        }
        if(parsed < static_cast<long double>(std::numeric_limits<ResTy>::lowest()) ||
           parsed > static_cast<long double>(std::numeric_limits<ResTy>::max())) {
            return "floating-point value out of range: " + std::string(text);
        }
        out = static_cast<ResTy>(parsed);
        return std::nullopt;
    } else if constexpr(trait::StringResultType<ResTy>) {
        out = ResTy(text);
        return std::nullopt;
    } else {
        static_assert(!sizeof(ResTy), "Unsupported scalar result type.");
        return "unsupported scalar result type";
    }
}

template <typename ResTy>
std::optional<std::string> assign_scalar(std::optional<ResTy>& target, std::string_view text) {
    if constexpr(trait::CustomStringResultTy<ResTy>) {
        auto& res = target.emplace();
        const auto err = res.into(text);
        return normalize_into_error(err);
    } else {
        ResTy parsed{};
        if(auto err = parse_primitive_scalar(parsed, text)) {
            return err;
        }
        target = std::move(parsed);
        return std::nullopt;
    }
}

template <typename ResTy>
std::optional<std::string> assign_vector(std::optional<ResTy>& target,
                                         std::span<const std::string_view> values) {
    if constexpr(trait::CustomStringVectorResultTy<ResTy>) {
        auto& res = target.emplace();
        std::vector<std::string_view> custom_values(values.begin(), values.end());
        const auto err = res.into(custom_values);
        return normalize_into_error(err);
    } else {
        using item_type = typename ResTy::value_type;
        ResTy parsed;
        parsed.clear();
        for(std::size_t i = 0; i < values.size(); ++i) {
            item_type item{};
            if(auto err = parse_primitive_scalar(item, values[i])) {
                return "invalid vector value at index " + std::to_string(i) + ": " + *err;
            }
            parsed.emplace_back(std::move(item));
        }
        target = std::move(parsed);
        return std::nullopt;
    }
}
}  // namespace detail

template <typename ResTy>
struct FlagOption : DecoOption<ResTy> {
    static_assert(trait::FlagResultType<ResTy>, "Flag result type must be bool or uint32_t.");
    using DecoOption<ResTy>::DecoOption;

    std::optional<std::string> into(backend::ParsedArgument&& arg) override {
        if(!arg.values.empty()) {
            return "flag option does not accept values";
        }
        if constexpr(std::same_as<ResTy, bool>) {
            this->value = true;
        } else {
            this->value = this->value.value_or(0) + 1;
        }
        return std::nullopt;
    }
};

template <typename ResTy>
struct ScalarOption : DecoOption<ResTy> {
    static_assert(trait::ScalarResultType<ResTy>, DecoScalarResultErrString);
    using DecoOption<ResTy>::DecoOption;

    std::optional<std::string> into(backend::ParsedArgument&& arg) override {
        if(arg.values.size() != 1) {
            return "expected exactly one value";
        }
        return detail::assign_scalar(this->value, arg.values.front());
    }
};

template <typename ResTy>
struct InputOption : DecoOption<ResTy> {
    static_assert(trait::ScalarResultType<ResTy>, DecoScalarResultErrString);
    using DecoOption<ResTy>::DecoOption;

    std::optional<std::string> into(backend::ParsedArgument&& arg) override {
        if(arg.values.empty()) {
            return detail::assign_scalar(this->value, arg.get_spelling_view());
        }
        if(arg.values.size() == 1) {
            return detail::assign_scalar(this->value, arg.values.front());
        }
        return "input option expects at most one value";
    }
};

template <typename ResTy>
struct VectorOption : DecoOption<ResTy> {
    static_assert(trait::VectorResultType<ResTy>, DecoVectorResultErrString);
    using DecoOption<ResTy>::DecoOption;

    std::optional<std::string> into(backend::ParsedArgument&& arg) override {
        return detail::assign_vector(this->value, arg.values);
    }
};

struct SubCommand {
    std::string_view name;
    std::string_view description;
    // if null, use name as subcommand
    std::optional<std::string_view> command;
};

}  // namespace deco::decl
