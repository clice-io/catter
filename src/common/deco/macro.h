#pragma once
#include "decl.h"
#include <concepts>
#include <type_traits>
#include <utility>

#define DECO_CONCAT_IMPL(a, b) a##b
#define DECO_CONCAT(a, b) DECO_CONCAT_IMPL(a, b)
#define DECO_STRUCT_NAME(id) DECO_CONCAT(_DecoStruct_, id)

#define DECO_EXTRA_NONE(id)

#define DECO_EXTRA_VALUE_TEMPLATE(id)                                                              \
    template <typename DefaultTy>                                                                  \
        requires (!std::same_as<std::remove_cvref_t<DefaultTy>, DECO_STRUCT_NAME(id)> &&           \
                  std::constructible_from<ResTy, DefaultTy>)                                       \
    constexpr DECO_STRUCT_NAME(id)(DefaultTy && default_value) : DECO_STRUCT_NAME(id)() {          \
        this->value = ResTy(std::forward<DefaultTy>(default_value));                               \
    }                                                                                              \
    template <typename DefaultTy>                                                                  \
        requires (!std::same_as<std::remove_cvref_t<DefaultTy>, DECO_STRUCT_NAME(id)> &&           \
                  std::constructible_from<ResTy, DefaultTy>)                                       \
    constexpr auto operator= (DefaultTy&& default_value)->DECO_STRUCT_NAME(id) & {                 \
        this->value = ResTy(std::forward<DefaultTy>(default_value));                               \
        return *this;                                                                              \
    }

#define DECO_EXTRA_VALUE_BOOL(id)                                                                  \
    template <typename DefaultTy>                                                                  \
        requires (!std::same_as<std::remove_cvref_t<DefaultTy>, DECO_STRUCT_NAME(id)> &&           \
                  std::convertible_to<DefaultTy, bool>)                                            \
    constexpr DECO_STRUCT_NAME(id)(DefaultTy && default_value) : DECO_STRUCT_NAME(id)() {          \
        this->value = static_cast<bool>(std::forward<DefaultTy>(default_value));                   \
    }                                                                                              \
    template <typename DefaultTy>                                                                  \
        requires (!std::same_as<std::remove_cvref_t<DefaultTy>, DECO_STRUCT_NAME(id)> &&           \
                  std::convertible_to<DefaultTy, bool>)                                            \
    constexpr auto operator= (DefaultTy&& default_value)->DECO_STRUCT_NAME(id) & {                 \
        this->value = static_cast<bool>(std::forward<DefaultTy>(default_value));                   \
        return *this;                                                                              \
    }

#define DECO_DECLARE_TYPED_IMPL(id, base_ty, using_block, init_block, extra_block_macro)           \
    struct DECO_STRUCT_NAME(id) : public base_ty {                                                 \
        using _deco_base_t = base_ty;                                                              \
        using_block constexpr DECO_STRUCT_NAME(id)() {                                             \
            init_block;                                                                            \
        }                                                                                          \
        extra_block_macro(id)                                                                      \
    };                                                                                             \
    DECO_STRUCT_NAME(id)

#define DECO_DECLARE_TYPED(base_ty, using_block, init_block, extra_block_macro)                    \
    DECO_DECLARE_TYPED_IMPL(__COUNTER__, base_ty, using_block, init_block, extra_block_macro)

#define DECO_DECLARE_TEMPLATE_IMPL(id, base_tpl, using_block, init_block, extra_block_macro)       \
    template <typename ResTy>                                                                      \
    struct DECO_STRUCT_NAME(id) : public base_tpl<ResTy> {                                         \
        using _deco_base_t = base_tpl<ResTy>;                                                      \
        using_block constexpr DECO_STRUCT_NAME(id)() {                                             \
            init_block;                                                                            \
        }                                                                                          \
        extra_block_macro(id)                                                                      \
    };                                                                                             \
    DECO_STRUCT_NAME(id)

#define DECO_DECLARE_TEMPLATE(base_tpl, using_block, init_block, extra_block_macro)                \
    DECO_DECLARE_TEMPLATE_IMPL(__COUNTER__, base_tpl, using_block, init_block, extra_block_macro)

#define DECO_USING_COMMON                                                                          \
    using _deco_base_t::help;                                                                      \
    using _deco_base_t::required;

#define DECO_USING_NAMED                                                                           \
    DECO_USING_COMMON                                                                              \
    using _deco_base_t::prefix;                                                                    \
    using _deco_base_t::name;

#define DECO_USING_FLAG                                                                            \
    DECO_USING_NAMED                                                                               \
    using _deco_base_t::value;

#define DECO_USING_INPUT                                                                           \
    DECO_USING_COMMON                                                                              \
    using _deco_base_t::value;

#define DECO_USING_PACK                                                                            \
    DECO_USING_COMMON                                                                              \
    using _deco_base_t::value;

#define DECO_USING_KV                                                                              \
    DECO_USING_NAMED                                                                               \
    using _deco_base_t::style;                                                                     \
    using _deco_base_t::alias;                                                                     \
    using _deco_base_t::value;

#define DECO_USING_COMMA_JOINED                                                                    \
    DECO_USING_NAMED                                                                               \
    using _deco_base_t::alias;                                                                     \
    using _deco_base_t::value;

#define DECO_USING_MULTI                                                                           \
    DECO_USING_NAMED                                                                               \
    using _deco_base_t::arg_num;                                                                   \
    using _deco_base_t::alias;                                                                     \
    using _deco_base_t::value;

// bool result_type, no <...>.
#define DecoFlag(opt_name, ...)                                                                    \
    DECO_DECLARE_TYPED(deco::decl::FlagOption, DECO_USING_FLAG, name = #opt_name;                  \
                       __VA_ARGS__, DECO_EXTRA_VALUE_BOOL)

// usage: DecoInput(... )<ResTy> field{};
#define DecoInput(...)                                                                             \
    DECO_DECLARE_TEMPLATE(deco::decl::InputOption,                                                 \
                          DECO_USING_INPUT,                                                        \
                          __VA_ARGS__,                                                             \
                          DECO_EXTRA_VALUE_TEMPLATE)

// usage: DecoPack(... )<ResTy> field{};
#define DecoPack(...)                                                                              \
    DECO_DECLARE_TEMPLATE(deco::decl::PackOption,                                                  \
                          DECO_USING_PACK,                                                         \
                          __VA_ARGS__,                                                             \
                          DECO_EXTRA_VALUE_TEMPLATE)

#define DecoKVStyled(opt_name, kv_style, ...)                                                      \
    DECO_DECLARE_TEMPLATE(deco::decl::KVOption, DECO_USING_KV, name = #opt_name; style = kv_style; \
                          __VA_ARGS__, DECO_EXTRA_VALUE_TEMPLATE)

#define DecoKV(opt_name, ...) DecoKVStyled(opt_name, deco::decl::KVStyle::Separate, __VA_ARGS__)

#define DecoComma(opt_name, ...)                                                                   \
    DECO_DECLARE_TEMPLATE(                                                                         \
        deco::decl::CommaJoinedOption, DECO_USING_COMMA_JOINED, name = #opt_name;                  \
        __VA_ARGS__, DECO_EXTRA_VALUE_TEMPLATE)

#define DecoMulti(opt_name, number, ...)                                                           \
    DECO_DECLARE_TEMPLATE(deco::decl::MultiOption, DECO_USING_MULTI, name = #opt_name;             \
                          arg_num = number;                                                        \
                          __VA_ARGS__, DECO_EXTRA_VALUE_TEMPLATE)
