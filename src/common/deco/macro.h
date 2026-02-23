#pragma once
#include "deco/decl.h"
#include "deco/trait.h"

#include <type_traits>

#define DECO_CONCAT_IMPL(a, b) a##b
#define DECO_CONCAT(a, b) DECO_CONCAT_IMPL(a, b)
#define DECO_CFG_STRUCT_NAME(id) DECO_CONCAT(_DecoCfgStruct_, id)
#define DECO_CFG_NAME(id) DECO_CONCAT(__deco_cfg_wrapper, id)
#define DECO_OPTION_STRUCT_NAME(id) DECO_CONCAT(_DecoOptStruct_, id)

#define DECO_USING_OPTION_FIELDS                                                                   \
    using _deco_base_t::required;                                                                  \
    using _deco_base_t::category;

#define DECO_USING_COMMON                                                                          \
    DECO_USING_OPTION_FIELDS                                                                       \
    using _deco_base_t::help;                                                                      \
    using _deco_base_t::meta_var;

#define DECO_USING_NAMED                                                                           \
    DECO_USING_COMMON                                                                              \
    using _deco_base_t::names;

#define DECO_USING_FLAG DECO_USING_NAMED
#define DECO_USING_INPUT DECO_USING_COMMON
#define DECO_USING_PACK DECO_USING_COMMON

#define DECO_USING_KV                                                                              \
    DECO_USING_NAMED                                                                               \
    using _deco_base_t::style;

#define DECO_USING_COMMA_JOINED DECO_USING_NAMED

#define DECO_USING_MULTI                                                                           \
    DECO_USING_NAMED                                                                               \
    using _deco_base_t::arg_num;

#define DECO_CONFIG_IMPL(id, TY, ...)                                                              \
    struct DECO_CFG_STRUCT_NAME(id) : public deco::decl::ConfigFields {                            \
        using _deco_base_t = deco::decl::ConfigFields;                                             \
        DECO_USING_COMMON                                                                          \
        constexpr DECO_CFG_STRUCT_NAME(id)() {                                                     \
            type = TY;                                                                             \
            __VA_ARGS__;                                                                           \
        }                                                                                          \
    };                                                                                             \
    DECO_CFG_STRUCT_NAME(id) DECO_CFG_NAME(id);

#define DECO_CFG(...)                                                                              \
    DECO_CONFIG_IMPL(__COUNTER__, deco::decl::ConfigFields::Type::Next, __VA_ARGS__)
#define DECO_CFG_START(...)                                                                        \
    DECO_CONFIG_IMPL(__COUNTER__, deco::decl::ConfigFields::Type::Start, __VA_ARGS__)
#define DECO_CFG_END(...)                                                                          \
    DECO_CONFIG_IMPL(__COUNTER__, deco::decl::ConfigFields::Type::End, __VA_ARGS__)

#define DECO_DECLARE_OPTION_TYPED_IMPL(id, option_base_ty, cfg_base_ty, using_block, ...)          \
    struct DECO_OPTION_STRUCT_NAME(id) : public option_base_ty {                                   \
        struct __cfg_ty : public cfg_base_ty {                                                     \
            using _deco_base_t = cfg_base_ty;                                                      \
            using_block constexpr __cfg_ty() {                                                     \
                __VA_ARGS__;                                                                       \
            }                                                                                      \
        };                                                                                         \
        constexpr static auto deco_field_ty = __cfg_ty::deco_field_ty;                             \
        using _deco_base_t = option_base_ty;                                                       \
        using _deco_base_t::_deco_base_t;                                                          \
    };                                                                                             \
    DECO_OPTION_STRUCT_NAME(id)

#define DECO_DECLARE_OPTION_TYPED(option_base_ty, cfg_base_ty, using_block, ...)                   \
    DECO_DECLARE_OPTION_TYPED_IMPL(__COUNTER__,                                                    \
                                   option_base_ty,                                                 \
                                   cfg_base_ty,                                                    \
                                   using_block,                                                    \
                                   __VA_ARGS__)

#define DECO_DECLARE_OPTION_TEMPLATE_IMPL(id,                                                      \
                                          res_concept,                                             \
                                          default_res_type,                                        \
                                          option_base_tpl,                                         \
                                          cfg_base_ty,                                             \
                                          using_block,                                             \
                                          ...)                                                     \
    template <res_concept ResTy = default_res_type>                                                \
    struct DECO_OPTION_STRUCT_NAME(id) : public option_base_tpl<ResTy> {                           \
        struct __cfg_ty : public cfg_base_ty {                                                     \
            using _deco_base_t = cfg_base_ty;                                                      \
            using_block constexpr __cfg_ty() {                                                     \
                __VA_ARGS__;                                                                       \
            }                                                                                      \
        };                                                                                         \
        constexpr static auto deco_field_ty = __cfg_ty::deco_field_ty;                             \
        using _deco_base_t = option_base_tpl<ResTy>;                                               \
        using _deco_base_t::_deco_base_t;                                                          \
    };                                                                                             \
    template <typename DefaultTy>                                                                  \
    DECO_OPTION_STRUCT_NAME(id)(DefaultTy&&)                                                       \
        ->DECO_OPTION_STRUCT_NAME(id)<std::remove_cvref_t<DefaultTy>>;                             \
    DECO_OPTION_STRUCT_NAME(id)

#define DECO_DECLARE_OPTION_TEMPLATE(res_concept,                                                  \
                                     default_res_type,                                             \
                                     option_base_tpl,                                              \
                                     cfg_base_ty,                                                  \
                                     using_block,                                                  \
                                     ...)                                                          \
    DECO_DECLARE_OPTION_TEMPLATE_IMPL(__COUNTER__,                                                 \
                                      res_concept,                                                 \
                                      default_res_type,                                            \
                                      option_base_tpl,                                             \
                                      cfg_base_ty,                                                 \
                                      using_block,                                                 \
                                      __VA_ARGS__)

#define DecoFlag(...)                                                                              \
    DECO_DECLARE_OPTION_TYPED(deco::decl::FlagOption<bool>,                                        \
                              deco::decl::FlagFields,                                              \
                              DECO_USING_FLAG,                                                     \
                              __VA_ARGS__)

#define DecoInput(...)                                                                             \
    DECO_DECLARE_OPTION_TEMPLATE(deco::trait::ScalarResultType,                                    \
                                 std::string,                                                      \
                                 deco::decl::InputOption,                                          \
                                 deco::decl::InputFields,                                          \
                                 DECO_USING_INPUT,                                                 \
                                 __VA_ARGS__)

#define DecoPack(...)                                                                              \
    DECO_DECLARE_OPTION_TEMPLATE(deco::trait::VectorResultType,                                    \
                                 std::vector<std::string>,                                         \
                                 deco::decl::VectorOption,                                         \
                                 deco::decl::PackFields,                                           \
                                 DECO_USING_PACK,                                                  \
                                 __VA_ARGS__)

#define DecoKVStyled(kv_style, ...)                                                                \
    DECO_DECLARE_OPTION_TEMPLATE(deco::trait::ScalarResultType,                                    \
                                 std::string,                                                      \
                                 deco::decl::ScalarOption,                                         \
                                 deco::decl::KVFields,                                             \
                                 DECO_USING_KV,                                                    \
                                 style = kv_style;                                                 \
                                 __VA_ARGS__)

#define DecoKV(...) DecoKVStyled(deco::decl::KVStyle::Separate, __VA_ARGS__)

#define DecoComma(...)                                                                             \
    DECO_DECLARE_OPTION_TEMPLATE(deco::trait::VectorResultType,                                    \
                                 std::vector<std::string>,                                         \
                                 deco::decl::VectorOption,                                         \
                                 deco::decl::CommaJoinedFields,                                    \
                                 DECO_USING_COMMA_JOINED,                                          \
                                 __VA_ARGS__)

#define DecoMulti(number, ...)                                                                     \
    DECO_DECLARE_OPTION_TEMPLATE(deco::trait::VectorResultType,                                    \
                                 std::vector<std::string>,                                         \
                                 deco::decl::VectorOption,                                         \
                                 deco::decl::MultiFields,                                          \
                                 DECO_USING_MULTI,                                                 \
                                 arg_num = number;                                                 \
                                 __VA_ARGS__)
