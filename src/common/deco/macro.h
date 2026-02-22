#pragma once
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

#define DECO_DECLARE_TYPED_IMPL(id, base_ty, using_block, extra_block_macro, ...)                  \
    struct DECO_STRUCT_NAME(id) : public base_ty {                                                 \
        using _deco_base_t = base_ty;                                                              \
        using_block constexpr DECO_STRUCT_NAME(id)() {                                             \
            __VA_ARGS__;                                                                           \
        }                                                                                          \
        extra_block_macro(id)                                                                      \
    };                                                                                             \
    DECO_STRUCT_NAME(id)

#define DECO_DECLARE_TYPED(base_ty, using_block, extra_block_macro, ...)                           \
    DECO_DECLARE_TYPED_IMPL(__COUNTER__, base_ty, using_block, extra_block_macro, __VA_ARGS__)

#define DECO_DECLARE_TEMPLATE_IMPL(id, base_tpl, using_block, extra_block_macro, ...)              \
    template <typename ResTy>                                                                      \
    struct DECO_STRUCT_NAME(id) : public base_tpl<ResTy> {                                         \
        using _deco_base_t = base_tpl<ResTy>;                                                      \
        using_block constexpr DECO_STRUCT_NAME(id)() {                                             \
            __VA_ARGS__;                                                                           \
        }                                                                                          \
        extra_block_macro(id)                                                                      \
    };                                                                                             \
    template <typename DefaultTy>                                                                  \
    DECO_STRUCT_NAME(id)(DefaultTy&&)->DECO_STRUCT_NAME(id)<std::remove_cvref_t<DefaultTy>>;       \
    DECO_STRUCT_NAME(id)

#define DECO_DECLARE_TEMPLATE(base_tpl, using_block, extra_block_macro, ...)                       \
    DECO_DECLARE_TEMPLATE_IMPL(__COUNTER__, base_tpl, using_block, extra_block_macro, __VA_ARGS__)

#define DECO_USING_OPTION_FIELDS                                                                   \
    using _deco_base_t::required;                                                                  \
    using _deco_base_t::exclusive;                                                                 \
    using _deco_base_t::category;

#define DECO_USING_COMMON                                                                          \
    DECO_USING_OPTION_FIELDS                                                                       \
    using _deco_base_t::help;                                                                      \
    using _deco_base_t::meta_var;

#define DECO_USING_NAMED                                                                           \
    DECO_USING_COMMON                                                                              \
    using _deco_base_t::names;

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
    using _deco_base_t::value;

#define DECO_USING_COMMA_JOINED                                                                    \
    DECO_USING_NAMED                                                                               \
    using _deco_base_t::value;

#define DECO_USING_MULTI                                                                           \
    DECO_USING_NAMED                                                                               \
    using _deco_base_t::arg_num;                                                                   \
    using _deco_base_t::value;

#define DecoFlag(...)                                                                              \
    DECO_DECLARE_TYPED(deco::decl::FlagOption, DECO_USING_FLAG, DECO_EXTRA_VALUE_BOOL, __VA_ARGS__)

#define DECO_CONFIG_IMPL(TY, ...)                                                                  \
    DECO_DECLARE_TYPED(                                                                            \
        deco::decl::ConfigFields, DECO_USING_OPTION_FIELDS, DECO_EXTRA_NONE, __VA_ARGS__;          \
        this->type = TY;)                                                                          \
    DECO_STRUCT_NAME(__COUNTER__)

#define DECO_CFG(...) DECO_CONFIG_IMPL(deco::decl::ConfigFields::Type::Next, __VA_ARGS__)
#define DECO_CFG_START(...) DECO_CONFIG_IMPL(deco::decl::ConfigFields::Type::Start, __VA_ARGS__)
#define DECO_CFG_END(...) DECO_CONFIG_IMPL(deco::decl::ConfigFields::Type::End, __VA_ARGS__)
#define Deco_CFG_END(...) DECO_CFG_END(__VA_ARGS__)

#define DecoInput(...)                                                                             \
    DECO_DECLARE_TEMPLATE(deco::decl::InputOption,                                                 \
                          DECO_USING_INPUT,                                                        \
                          DECO_EXTRA_VALUE_TEMPLATE,                                               \
                          __VA_ARGS__)

// usage: DecoPack(... )<ResTy> field{};
#define DecoPack(...)                                                                              \
    DECO_DECLARE_TEMPLATE(deco::decl::PackOption,                                                  \
                          DECO_USING_PACK,                                                         \
                          DECO_EXTRA_VALUE_TEMPLATE,                                               \
                          __VA_ARGS__)

#define DecoKVStyled(kv_style, ...)                                                                \
    DECO_DECLARE_TEMPLATE(                                                                         \
        deco::decl::KVOption, DECO_USING_KV, DECO_EXTRA_VALUE_TEMPLATE, style = kv_style;          \
        __VA_ARGS__)

#define DecoKV(...) DecoKVStyled(deco::decl::KVStyle::Separate, __VA_ARGS__)

#define DecoComma(...)                                                                             \
    DECO_DECLARE_TEMPLATE(deco::decl::CommaJoinedOption,                                           \
                          DECO_USING_COMMA_JOINED,                                                 \
                          DECO_EXTRA_VALUE_TEMPLATE,                                               \
                          __VA_ARGS__)

#define DecoMulti(number, ...)                                                                     \
    DECO_DECLARE_TEMPLATE(                                                                         \
        deco::decl::MultiOption, DECO_USING_MULTI, DECO_EXTRA_VALUE_TEMPLATE, arg_num = number;    \
        __VA_ARGS__)
