#pragma once
#include "decl.h"
#include <type_traits>

namespace deco::ty {

template <typename T>
concept is_decoed = std::is_base_of_v<deco::decl::DecoFields, T> && requires {
    T::decoTy;
    typename T::result_type;
};
}  // namespace deco::ty
