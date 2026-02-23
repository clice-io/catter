#pragma once
#include "decl.h"
#include "reflection/struct.h"
#include "util/meta.h"
#include <type_traits>
#include <utility>

namespace deco::ty {

template <typename T>
using base_ty = std::remove_cvref_t<T>;

template <typename T>
concept is_deco_field = std::is_base_of_v<decl::DecoFields, base_ty<T>>;

template <typename T>
concept is_config_field = std::is_base_of_v<decl::ConfigFields, base_ty<T>>;

template <typename T>
concept is_option_cfg_field =
    std::is_base_of_v<decl::CommonOptionFields, base_ty<T>> && !is_config_field<T> && requires {
        { base_ty<T>::deco_field_ty } -> std::convertible_to<decl::DecoType>;
    };

template <typename T>
concept deco_option_like = requires {
    requires std::is_base_of_v<decl::DecoOptionBase, base_ty<T>>;
    typename base_ty<T>::result_type;
    typename base_ty<T>::__cfg_ty;
    requires is_option_cfg_field<typename base_ty<T>::__cfg_ty>;
    { base_ty<T>::deco_field_ty } -> std::convertible_to<decl::DecoType>;
};

template <typename T>
concept is_option_field = is_option_cfg_field<T> || deco_option_like<T>;

template <typename T>
concept is_decoed = is_config_field<T> || deco_option_like<T>;

template <typename T>
using cfg_ty_of = typename base_ty<T>::__cfg_ty;

template <ty::is_option_field T>
constexpr auto dyn_cast(const T& field) {
    if constexpr(deco_option_like<T>) {
        return cfg_ty_of<T>{};
    } else {
        return field;
    }
}
}  // namespace deco::ty
